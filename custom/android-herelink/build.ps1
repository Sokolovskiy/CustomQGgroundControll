param(
    [string]$QtRoot,
    [string]$AndroidSdk,
    [string]$JavaHome,
    [string]$CMake,
    [string]$Ninja,
    [int]$Jobs = 8
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$tools = Join-Path $repo '.cache/herelink-toolchain'
if (!$QtRoot) { $QtRoot = Join-Path $tools 'Qt/6.6.3' }
if (!$AndroidSdk) { $AndroidSdk = Join-Path $tools 'sdk' }
if (!$JavaHome) { $JavaHome = Join-Path $tools 'java' }
if (!$CMake) {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) { $CMake = $command.Source }
    else {
        $CMake = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    }
}
if (!$Ninja) {
    $command = Get-Command ninja -ErrorAction SilentlyContinue
    if ($command) { $Ninja = $command.Source }
    else {
        $Ninja = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
    }
}
$python = Join-Path $repo '.venv/Scripts/python.exe'
$source = Join-Path $repo '.cache/herelink-android-source'
$build = Join-Path $repo 'build-herelink-android'
$ndk = Join-Path $AndroidSdk 'ndk/25.1.8937393'
foreach ($required in @($CMake, $Ninja, $python, "$QtRoot/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake",
    "$QtRoot/msvc2019_64/bin/moc.exe", "$JavaHome/bin/java.exe", "$ndk/build/cmake/android.toolchain.cmake")) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing build dependency: $required. See custom/android-herelink/README.md" }
}

& (Join-Path $PSScriptRoot 'generate-icons.ps1')
& $python (Join-Path $PSScriptRoot 'prepare.py') --source $source
if ($LASTEXITCODE) { throw 'Preparing Android sources failed' }
& git -C $source submodule update --init --depth 1
if ($LASTEXITCODE) { throw 'Fetching CubePilot submodules failed' }

$env:JAVA_HOME = $JavaHome
$env:ANDROID_SDK_ROOT = $AndroidSdk
$env:ANDROID_HOME = $AndroidSdk
$env:ANDROID_NDK_ROOT = $ndk
$env:ANDROID_NDK_HOME = $ndk
$env:ANDROID_NDK = $ndk
$env:GRADLE_OPTS = "$env:GRADLE_OPTS -Dorg.gradle.daemon=false"
$env:PATH = "$JavaHome/bin;$(Split-Path $Ninja);$(Split-Path $CMake);$env:PATH"
& $CMake -S $source -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" `
    "-DCMAKE_TOOLCHAIN_FILE=$QtRoot/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake" `
    "-DQT_HOST_PATH=$QtRoot/msvc2019_64" "-DANDROID_SDK_ROOT=$AndroidSdk" `
    "-DANDROID_NDK=$ndk" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-25 `
    -DCMAKE_BUILD_TYPE=Release -DQGC_ENABLE_HERELINK=ON -DQGC_BUILD_TESTING=OFF `
    -DQT_ANDROID_ABIS=arm64-v8a -DQT_ANDROID_BUILD_ALL_ABIS=OFF -DQT_ANDROID_SIGN_APK=OFF `
    -DQGC_ENABLE_GST_VIDEOSTREAMING=ON "-DPython3_EXECUTABLE=$python"
if ($LASTEXITCODE) { throw 'Android CMake configure failed' }
& $CMake --build $build --parallel $Jobs
if ($LASTEXITCODE) { throw 'Android build failed' }

$unsignedApk = Join-Path $build 'android-build/FatumHerelink.apk'
if (!(Test-Path -LiteralPath $unsignedApk)) { throw "Build produced no APK at $unsignedApk" }
$keystore = Join-Path $tools 'fatum-debug.keystore'
if (!(Test-Path -LiteralPath $keystore)) {
    & "$JavaHome/bin/keytool.exe" -genkeypair -keystore $keystore -alias androiddebugkey `
        -storepass android -keypass android -keyalg RSA -keysize 2048 -validity 10000 `
        -dname 'CN=Fatum Android Development,O=Fatum,C=UA' -noprompt
    if ($LASTEXITCODE) { throw 'Creating the local development signing key failed' }
}
$alignedApk = Join-Path $build 'FatumHerelink-aligned.apk'
$signedApk = Join-Path $build 'FatumHerelink-android7-arm64.apk'
$buildTools = Join-Path $AndroidSdk 'build-tools/35.0.0'
& "$buildTools/zipalign.exe" -f -p 4 $unsignedApk $alignedApk
if ($LASTEXITCODE) { throw 'Aligning the APK failed' }
& "$buildTools/apksigner.bat" sign --ks $keystore --ks-key-alias androiddebugkey `
    --ks-pass pass:android --key-pass pass:android --out $signedApk $alignedApk
if ($LASTEXITCODE) { throw 'Signing the APK failed' }
& "$buildTools/apksigner.bat" verify --verbose $signedApk
if ($LASTEXITCODE) { throw 'Verifying the APK signature failed' }
Get-Item -LiteralPath $signedApk | Select-Object FullName, Length
