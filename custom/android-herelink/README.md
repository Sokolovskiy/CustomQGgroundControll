# Herelink Android 7.1.2 display port

This is a separate Android build of the FATUM display on CubePilot's native Herelink QGC.
The Windows source, build directories and Qt installation are independent of this port.

## Source and control behavior

`prepare.py` prepares a dedicated checkout of
[CubePilot Stable_V5.0-Herelink](https://github.com/CubePilot/qgroundcontrol-herelink/tree/Stable_V5.0-Herelink),
pinned to `eff16bdf347b8aba6bd7cdbb2e25a306adf96ac7`.
The generated checkout is disposable: edit the files in this directory instead.
Running preparation again regenerates the display files and plugin integration from the pinned originals.
The otherwise floating `libevents` dependency is pinned to
`840a88ea226d4eb0fd4c391ce860317422756435`, the revision available at the CubePilot release date.
Later `libevents` revisions require MAVLink helpers absent from this base.
Preparation also supplies the missing `libpng.pc` alias in the Android GStreamer SDK, so its
OpenGL plugin links with its declared static dependencies instead of the finder's archive fallback.

The existing CubePilot plugin still owns connection defaults, joystick selection, buttons, camera
switching, flight modes, arming, warnings and confirmations. The only plugin addition installs an
engine-owned presentation resource interceptor and registers a passive telemetry reader.
No Windows joystick handlers, command repeats, flight-mode filters, parameter writes or camera
routing logic are copied. The display uses native Facts and listens to existing MAVLink traffic.

The OSD appears in video view and anchors to the viewport edges, independently of corner-widget
insets. The native bottom-right compass and telemetry bar are hidden in video view and retained
on the map. Native action buttons sit below the OSD mode badge in video view; their actions are
unchanged. Native PiP and its map toggle sit above the lower-left OSD in video view. Virtual
joysticks, warnings and confirmation controls remain native.
The OSD button toggles only this display. Native camera controls remain available.
The APK has its own package ID, `org.fatum.herelink.gcs`, and settings identity.
Its launcher icon uses the desktop `custom/resources/herelink-gcs.ico` artwork. The build runs
`generate-icons.ps1` to extract its largest PNG frame and generate all six legacy Android icon
densities; preparation overlays those PNGs into the native Android package template.
Close the stock QGC before using this app with the vehicle, since both use the native Herelink ports.

The desktop panels, colors, reticle, background, telemetry and 3D aircraft are reused. Qt 6.6 does
not support the desktop procedural horn mesh API, so this small decorative part uses a primitive.
RC indicators show received channels normalized through the aircraft's available RCMAP/RC Facts;
they are indicators, not input controls. Missing mappings show no stick position. Radio status
is decoded only for the known Herelink radio service on the active vehicle's primary link.
RC, radio and attitude observations expire after 1.5 seconds and reset on vehicle/link changes.

The camera badge uses native reported HDMI state. Camera tilt and the FC SD-card callsign stay
unavailable until a suitable native source is provided; this build does not infer camera tilt from
Windows channel assignments or add an FTP request. The motion arrow uses fresh local velocity
with estimator validity flags, relative to the same initial heading used by the 3D model.
The 3D model combines roll, pitch and yaw from native Facts. Its first valid heading defines
the scene reference, which resets on vehicle change or loss of valid attitude. Unknown data is displayed as `—`.

## Toolchain

Match the pinned CubePilot Android workflow:

- Qt **6.6.3**, desktop `msvc2019_64` host tools and Android `android_arm64_v8a`.
- Qt modules: `qtcharts qtlocation qtpositioning qtspeech qt5compat qtmultimedia qtserialport
  qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors`.
- JDK **17**, Android NDK **25.1.8937393** (r25b).
- SDK platform **35**, build tools **35.0.0**, platform-tools.
- CMake 3.25 or newer, Ninja, Git and Python 3.10 or newer.
- GStreamer **1.22.12** for Android is fetched by the pinned build.

The minimum and target runtime API remain **25**, as in CubePilot's Herelink configuration.
Compile SDK 35 does not change that minimum.

The default local layout is `.cache/herelink-toolchain/{Qt/6.6.3,java,sdk}`.
For example, Qt can be installed there using `aqtinstall`:

```powershell
.venv/Scripts/python.exe -m aqt install-qt windows desktop 6.6.3 win64_msvc2019_64 `
    -O .cache/herelink-toolchain/Qt -m qtcharts qtlocation qtpositioning qtspeech qt5compat `
    qtmultimedia qtserialport qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors
.venv/Scripts/python.exe -m aqt install-qt windows android 6.6.3 android_arm64_v8a `
    -O .cache/herelink-toolchain/Qt -m qtcharts qtlocation qtpositioning qtspeech qt5compat `
    qtmultimedia qtserialport qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors
```

## Build

From the main repository root in PowerShell:

```powershell
./custom/android-herelink/build.ps1
```

The script prepares sources, initializes the pinned submodule, configures the ARM64 Android build
and builds it with GStreamer enabled. `-QtRoot`, `-AndroidSdk`, `-JavaHome`, `-CMake`, `-Ninja`
and `-Jobs` override local paths and concurrency. It stops on configure/build errors.
The build directory is `build-herelink-android`. The script aligns, signs and verifies
`build-herelink-android/FatumHerelink-android7-arm64.apk` with a local development identity
stored in `.cache/herelink-toolchain/fatum-debug.keystore`, not a production release key.
Keep this key if you want to update the installed development app without uninstalling it.

## Display verification

The standalone Qt 6.6 display harness uses fake vehicle Facts and telemetry, with no command API.
It covers disconnection/reconnection, unknown data, native Fact readouts, expired observations,
camera display and loading/rendering the 3D model. It does not replace a device bench check.

After running `prepare.py`, configure `custom/android-herelink/test` with the Qt 6.6 desktop kit,
build `HerelinkDisplayCheck`, then run it with the Qt desktop `bin` directory on PATH:

```powershell
$env:QT_QUICK_CONTROLS_STYLE = 'Basic'
./.cache/herelink-display-check/HerelinkDisplayCheck.exe `
    -input custom/android-herelink/test -import custom/android-herelink/test/imports `
    -o .cache/herelink-display-test.txt,txt
```

The visual test writes `.cache/herelink-android-preview.png` with synthetic data.

Verified on 2026-11-16: the Release ARM64 APK built and its v2/v3 signatures verified. Its manifest
reports package `org.fatum.herelink.gcs`, version `5.0.8`, and minimum/target SDK 25. The Qt 6.6.3
display harness passed all seven results, including rendering, disconnection and virtual-joystick
visibility. Focused format, QML, secret, typo, CMake and vehicle-null checks passed. The full
pre-commit entry still expects a compilation database in the unrelated default `build` directory
for clang-tidy; the Android database is in `build-herelink-android`.
Clang-tidy 22 also rejects the pinned upstream `Vehicle.h` enum sentinel initializers, whereas
the required NDK Clang 14 compiles them. Those upstream control headers were left unchanged.
The initial device startup failed because the intercepted FlyView layer resolved sibling types
relative to the stock resource URL. The layer now explicitly imports `qrc:/Herelink`; the display
harness also loads it through that stock URL and the actual interceptor (eight passing results).
Preparation guards the upstream Quit handler against a missing root window after startup errors.
The corrected APK was installed over ADB on an Optimus running Android 7.1.2 and successfully
opened the main window and native measurement-units setup. Video, RC controls and flight behavior
still require the separate bench checks below.
The edge-layout regression gives the intercepted layer nonzero native corner insets and checks
that its OSD panels remain at the viewport edges (nine passing harness results).
The edge layout was installed and visually checked over ADB: the video OSD sits at the edges,
the duplicate compass/telemetry are absent, and the map toggle clears the stick panel.
Yaw regression checks cover a 90-degree turn, heading wrap through north, combined roll/pitch,
and resetting the heading reference after stale attitude (eleven passing harness results).
The yaw update was installed over ADB; the user confirmed that the model turns with the physical
aircraft on the bench while disarmed.
A later device crash was captured before this yaw update: SIGABRT from an invalid
free in `QObject::event`, preceded by speech callbacks. Its cause and reproduction remain open;
it is distinct from the resolved QML startup error. Logs are retained in the local cache.

## Install and bench check

Enable USB debugging on the Herelink and connect USB. First verify its runtime and ABI:

```powershell
adb devices
adb shell getprop ro.build.version.release
adb shell getprop ro.product.cpu.abilist
adb install <signed-apk-path>
```

The connected device must support `arm64-v8a`. Inspect the final APK manifest and signature before
installation. Installation alongside stock QGC uses a separate application/settings identity.
Run one GCS at a time. On the bench, verify video, telemetry, PiP, text size, camera switching,
native buttons, flight-mode selection and arming confirmations, as well as behavior on link loss.
Flight-controller parameters are not changed by preparation or installation.
