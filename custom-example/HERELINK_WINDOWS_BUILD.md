# Herelink GCS: Windows Debug Build Baseline

Verified on 2026-11-01 against upstream QGroundControl tag `v5.1.5`
(`3a67d31f0c36bf3fe38ec52970d250a89d0aaf67`). This document distinguishes
observed local state from work that is still proposed.

## Upstream requirements for this tag

The source of truth is `.github/build-config.json` at `v5.1.5`. Do not replace
these versions with whatever happens to be newest:

- Windows 11 x64.
- Visual Studio 2022 Build Tools with the C++/VCTools workload, or Visual
  Studio 2022 Community with Desktop development with C++.
- Qt **6.11.1** and the `msvc2022_64` kit. The repository requests these Qt
  add-ons: Graphs, Location, Positioning, TextToSpeech, Multimedia,
  SerialPort, ImageFormats, ShaderTools, Connectivity, Quick3D, Sensors,
  StateMachine, WebSockets, and HttpServer. The official GUI instructions say
  to select all Additional Libraries.
- CMake 3.25 or newer and Ninja.
- Python 3.10 or newer. CI uses Python 3.12 and the repository's locked tools
  environment; using Python 3.12 is the reproducible choice.
- GStreamer **1.28.4** MSVC x86_64 development runtime for video, including
  RTSP, RTP, H.264/libav, OpenH264, UDP/TCP, and the Windows video plugins.
- Git. A real clone should be used instead of a GitHub source archive.
- NSIS 3.11 only when producing a Windows installer. It is not required for a
  Debug executable.
- Vulkan SDK, Qt Creator, `just`, and compiler caches are optional for this
  Debug baseline.

## Local dependency audit (verified)

| Dependency | Local result |
| --- | --- |
| Checkout | HEAD resolves to `v5.1.5` / `3a67d31`; `custom-example/` exists and `custom/` does not |
| Git | 2.54.0.windows.1 |
| Python | 3.14.6 at `C:\Python314\python.exe`; Jinja2 3.1.6 is available |
| Required Python baseline | Python 3.12 environment is not present |
| Visual Studio 2022 | Not found |
| Other MSVC | VS Build Tools 2026 18.8.2, MSVC 19.51.36252, is installed; this is not the required toolchain |
| Windows SDK | 10.0.26100.0 is installed |
| CMake | Not on the normal PATH; VS 2026 bundles CMake 4.3.1 |
| Ninja | Not on the normal PATH; VS 2026 bundles Ninja 1.13.2 |
| Qt 6.11.1 `msvc2022_64` | Not found in `C:\Qt` or the user Qt directory |
| GStreamer 1.28.4 | Not found in the standard MSVC locations; no GStreamer environment variables are set |
| NSIS | Not found; optional for this task |
| `just` | Not found; optional because the direct CMake workflow below is sufficient |

No software was installed during this audit.

## Upstream build attempt (verified)

The unmodified source was configured into
`custom-example\build-upstream-debug` so the source tree did not need a
`custom/` directory and no product code was changed. The attempted generator
was `Ninja Multi-Config`, Debug only, with tests enabled.

CMake read the pinned Qt 6.11.1 and GStreamer 1.28.4 configuration, then failed
while checking the installed VS 2026 compiler, before Qt discovery:

```text
The C compiler ... cl.exe is not able to compile a simple test program.
fatal error C1902: Program database manager mismatch; please check your installation
```

This is a configure failure, not a QGroundControl C++ or QML compilation
failure. No QGroundControl target was built or linked. The installed compiler
is also the wrong major Visual Studio version for this tag. After installing
the required VS 2022 toolchain, Qt 6.11.1 kit, and GStreamer 1.28.4, repeat the
clean workflow below rather than reusing this failed CMake cache.

## Reproducible unmodified Debug build

The commands below are for an **x64 Native Tools Command Prompt for VS 2022**.
Use a clean clone or confirm that no directory named `custom` exists, because
QGroundControl automatically enables a custom build whenever that directory is
present.

Installation commands are shown for completeness but must only be run after
approval because the Windows dependency script changes machine-wide software
and environment variables.

```bat
git clone --branch v5.1.5 https://github.com/mavlink/qgroundcontrol.git herelink-gcs
cd herelink-gcs
git rev-parse HEAD
git status --short
if exist custom (echo ERROR: custom overlay is present & exit /b 1)

rem Run only after approval; installs the pinned GStreamer 1.28.4 SDK.
py -3.12 tools\setup\install_dependencies --platform windows

rem Optional installer prerequisite; not needed for Debug execution.
rem py -3.12 tools\setup\install_dependencies --platform windows --nsis

rem Create repository-local Python tooling. This does not install system-wide.
py -3.12 tools\setup\install_python.py dev
call .venv\Scripts\activate.bat

set "QT_ROOT_DIR=C:\Qt\6.11.1\msvc2022_64"
set "GSTREAMER_1_0_ROOT_MSVC_X86_64=C:\gstreamer\1.0\msvc_x86_64"
set "PATH=%GSTREAMER_1_0_ROOT_MSVC_X86_64%\bin;%PATH%"

if exist build-upstream-debug rmdir /s /q build-upstream-debug
"%QT_ROOT_DIR%\bin\qt-cmake.bat" -S . -B build-upstream-debug -G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Debug -DQGC_BUILD_TESTING=ON -DQGC_ENABLE_GST_VIDEOSTREAMING=ON -DPython3_EXECUTABLE="%CD%\.venv\Scripts\python.exe"
cmake --build build-upstream-debug --config Debug --parallel
ctest --test-dir build-upstream-debug -C Debug --output-on-failure -L Unit
build-upstream-debug\Debug\QGroundControl.exe
```

The direct upstream documentation uses `qt-cmake`, CMake, and Ninja; the
multi-config form above mirrors Windows CI and makes the executable path
unambiguous. Do not set `QGC_ENABLE_GST_VIDEOSTREAMING=OFF` for the Herelink
x64 build: that would invalidate RTSP/H.264 verification.

## First custom build plan (proposed, not implemented)

Create a tracked `custom/` overlay and leave upstream files unchanged. Start
from the custom example's integration mechanism, not its PX4-only product
assumptions:

1. Keep a small `custom/CMakeLists.txt`, `custom/cmake/CustomOverrides.cmake`,
   `custom/custom.qrc`, and minimal `CustomPlugin.{h,cc}`.
2. Set only Herelink branding/application identifiers initially. Do not disable
   transport, joystick, video, setup, warnings, preflight checks, guided safety
   actions, or failsafes.
3. Retain the URL interceptor from the example so custom QML resources under
   `/Custom/...` can replace upstream resources without editing upstream QML.
4. Omit the example's PX4 `CustomFirmwarePlugin`,
   `CustomFirmwarePluginFactory`, and `CustomAutoPilotPlugin` sources. Omit its
   perimeter-scan demonstration and custom guided action until they are actual
   Herelink requirements.
5. Keep both standard firmware factories registered. In particular, do not set
   `QGC_DISABLE_APM_PLUGIN_FACTORY=ON`. The first custom build should leave both
   `QGC_DISABLE_APM_PLUGIN_FACTORY` and `QGC_DISABLE_PX4_PLUGIN_FACTORY` at
   their upstream `OFF` defaults.
6. If an offline-planning default is desired, use
   `QGCMAVLink::FirmwareClassArduPilot`; do not copy the example's hidden PX4
   default. Keeping the setting visible is safer until the supported ArduPilot
   vehicle class is explicitly decided.
7. Build and hardware-smoke-test this branding-only overlay before changing Fly
   View. Verify ArduPilot parameter metadata/setup pages, telemetry, joystick,
   RTSP video, flight modes, messages, preflight checks, and safety actions.

Why the factory rule matters: the example explicitly sets
`QGC_DISABLE_APM_PLUGIN_FACTORY=ON`, disables the standard PX4 factory, and
registers a replacement factory that reports only PX4 multirotors. Copying it
unchanged would make `FirmwarePluginManager` omit ArduPilot support and could
fall back to the generic firmware plugin for an ArduPilot vehicle.

## Future video-first Fly View extension points (proposed)

Use these in increasing order of maintenance cost:

1. `QGCFlyViewOptions` in the custom core plugin can hide the standard
   instrument panel or map scale without copying the Fly View. Do not hide
   warnings or safety actions.
2. Override `QGroundControl/FlyView/FlyViewCustomLayer.qml` for additive
   telemetry/controls. Preserve `QGCToolInsets` propagation so map and controls
   do not overlap.
3. Override `FlyViewToolStripActionList.qml` only if the action list must be
   reduced or reordered. Keep preflight checklist, Land, RTL, and Pause unless
   a later safety review explicitly approves a change.
4. Override `CustomGuidedActionsController.qml` only to add real guided
   actions; the example's demonstration action should not ship.
5. Override `FlyViewWidgetLayer.qml` only when existing widgets must be
   repositioned. This copies more upstream layout and increases merge review.
6. Avoid overriding `FlyView.qml`. It owns map/video PiP state, guided controls,
   tool insets, and full-screen behavior. A video-first default can instead be
   seeded from the custom plugin by setting `QGCQml/MainFlyWindowIsMap=false`
   only when that preference is absent, preserving the user's later choice.

The upstream `FlyView.qml` already instantiates `FlyViewVideo` for
`videoManager.hasVideo`, and `PipView` persists the map-versus-video choice in
`MainFlyWindowIsMap`. This lets the first UI iteration stay above the existing
GStreamer decoder and VideoManager rather than modifying video decoding.

## Custom build commands (after the proposed overlay exists)

Use a separate build tree so upstream and custom cache state cannot mix:

```bat
cd C:\Projects\Camel\herelink-gcs
call .venv\Scripts\activate.bat
set "QT_ROOT_DIR=C:\Qt\6.11.1\msvc2022_64"
set "GSTREAMER_1_0_ROOT_MSVC_X86_64=C:\gstreamer\1.0\msvc_x86_64"
set "PATH=%GSTREAMER_1_0_ROOT_MSVC_X86_64%\bin;%PATH%"

if exist build-herelink-debug rmdir /s /q build-herelink-debug
"%QT_ROOT_DIR%\bin\qt-cmake.bat" -S . -B build-herelink-debug -G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Debug -DQGC_BUILD_TESTING=ON -DQGC_ENABLE_GST_VIDEOSTREAMING=ON -DPython3_EXECUTABLE="%CD%\.venv\Scripts\python.exe"
cmake --build build-herelink-debug --config Debug --parallel
ctest --test-dir build-herelink-debug -C Debug --output-on-failure -L Unit
build-herelink-debug\Debug\HerelinkGCS.exe
```

The final executable name depends on the exact `QGC_APP_NAME` chosen in
`CustomOverrides.cmake`; confirm it from CMake's configure summary rather than
assuming the sample name above.
