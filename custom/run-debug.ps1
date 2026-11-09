$ErrorActionPreference = 'Stop'
$sourceDirectory = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $sourceDirectory 'build-herelink-debug'
$executable = Join-Path $buildDirectory 'Debug\HerelinkGCS.exe'

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Application not found: $executable. Build with custom\build-debug.cmd first."
}
if (Get-Process -Name HerelinkGCS -ErrorAction SilentlyContinue) {
    throw 'Herelink GCS is already running. Close it safely, then run this command again.'
}

$env:PATH = 'C:\Qt\6.11.1\msvc2022_64\bin;C:\gstreamer\1.0\msvc_x86_64\bin;' + $env:PATH
$env:GSTREAMER_1_0_ROOT_MSVC_X86_64 = 'C:\gstreamer\1.0\msvc_x86_64'
$env:GSTREAMER_1_0_ROOT_X86_64 = $env:GSTREAMER_1_0_ROOT_MSVC_X86_64
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:QT_MESSAGE_PATTERN = '%{time yyyy-MM-dd hh:mm:ss.zzz} [%{type}] [%{category}] %{message}'

$logsDirectory = Join-Path $buildDirectory 'logs'
$sessionName = (Get-Date -Format 'yyyy-MM-dd_HH-mm-ss-fff') + '_' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$sessionDirectory = Join-Path $logsDirectory $sessionName
New-Item -ItemType Directory -Path $sessionDirectory -Force | Out-Null
$stdoutFile = Join-Path $sessionDirectory 'stdout.log'
$stderrFile = Join-Path $sessionDirectory 'commands.log'

# Use QGC's category filter: QT_LOGGING_RULES alone cannot enable these categories.
$categories = 'Herelink.ControlDiagnostics,Vehicle.MavCommandQueue,Vehicle,Joystick.Joystick,QMLControls.MavlinkAction,Comms.UDPLink'
$application = Start-Process -FilePath $executable -WorkingDirectory (Split-Path -Parent $executable) `
    -ArgumentList @('--logging', $categories) -RedirectStandardOutput $stdoutFile `
    -RedirectStandardError $stderrFile -PassThru

[ordered]@{
    started = (Get-Date).ToString('o')
    processId = $application.Id
    executable = $executable
    executableModifiedUtc = (Get-Item -LiteralPath $executable).LastWriteTimeUtc.ToString('o')
    loggingCategories = $categories
    commandLog = $stderrFile
    stdoutLog = $stdoutFile
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $sessionDirectory 'session.json') -Encoding UTF8
Set-Content -LiteralPath (Join-Path $logsDirectory 'latest-session.txt') -Value $sessionDirectory -Encoding UTF8
Write-Host "Herelink GCS started (PID $($application.Id))."
Write-Host "Command log: $stderrFile"
Write-Host 'After a switching error, note the local time and which switch/mode was selected.'
