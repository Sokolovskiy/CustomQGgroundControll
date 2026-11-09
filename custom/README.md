# Herelink GCS minimal overlay

For installation and first-time setup, see the [Ukrainian user guide](USER_GUIDE.uk.md).

## Windows release installer

Build from the repository root with the same x64 MSVC, Qt and GStreamer versions
as the Debug build below. Use an x64 MSVC developer shell and put the Qt,
GStreamer and NSIS 3.11 executables on `PATH`. Use the repository's configured
Python environment.

```powershell
qt-cmake.bat -S . -B build-herelink-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQGC_BUILD_TESTING=OFF -DQGC_ENABLE_GST_VIDEOSTREAMING=ON -DPython3_EXECUTABLE="$PWD/.venv/Scripts/python.exe"
cmake --build build-herelink-release --parallel
cmake --install build-herelink-release --config Release
```

The installer is generated in `build-herelink-release`. CMake stages Qt,
GStreamer and the compiler runtime with the application. The installer leaves
the existing QGroundControl application data directory in place on removal.

## Overlay history

Native main-window fullscreen is implemented through a custom replacement for
QGC's existing MainWindowSavedState helper. F11 toggles native Qt fullscreen
and restores the prior normal geometry/maximized mode; the preference survives
restarts using MainWindowState settings. It does not activate QGC's separate
video-only mode. See [FULLSCREEN.md](FULLSCREEN.md). The user subsequently
authorized a build and launch; both succeeded. Fullscreen/restore settings were
observed, but the complete geometry/restart checklist is still outstanding.
Automated tests were not run.

A passive Qt Quick 3D attitude widget is implemented in the custom OSD source.
Right-click or hold the existing OSD button for Show 3D Attitude (persisted,
default enabled). It requires fresh active-vehicle attitude, not GPS or a compass.
Yaw is relative. Implementation and the deferred manual checks are documented
in [ATTITUDE_3D.md](ATTITUDE_3D.md). After the user authorized builds, the Debug
application was built and launched successfully. Startup showed no QML loading
errors; the disconnected state hides the scene. Live rendering and physical
orientation still require verification. No automated tests were run.

Ordinary ArduPilot ARM/DISARM now permits one short repeat, explicitly approved
by the user on 2026-11-05. It is scheduled 100 ms after the initial send and
discarded at or beyond 250 ms. ACKs (including rejection/progress), newer arm
requests, observed armed/active-vehicle/link transitions and communication loss
cancel it. Force commands, handler-owned requests and high-latency links retain
the standard policy. No late retry or automatic force-arm is added. The deadline
limits local dispatch; it cannot recall packets already queued in the transport
or bound their delivery time. See COMMAND_DIAGNOSIS.md for the remaining limits.

For ArduPilot multirotors, the main toolbar's flight-mode drawer now lists only
Altitude Hold, Position Hold, Land and RTL, in that order and only when offered
by the vehicle. This is a custom resource override of the v5.1.5 toolbar
FlightModeIndicator.qml; the active mode label, confirmation behavior and action
handler remain standard. Mode-list editing is disabled for this restricted drawer
so old hidden-mode settings cannot hide these four choices or reveal others.
Other firmware/vehicle classes keep the standard menu. No flight-controller
parameters, available firmware modes, failsafes or joystick mappings are changed.
No automated tests are run for this UI change, per the user's instruction.

Mode and camera Relay2 OFF/ON delivery now uses up to three transmissions in
the custom build. A newer selection cancels retries of the old one and retains
only the latest intent until the existing ACK slot finishes. Deferred intent
expires after two seconds or a vehicle/link/armed-state change; final failure
remains visible. Relay Toggle, other relay indices and high-latency links keep
the standard policy; ARM/DISARM uses the separate short-window policy above.
See COMMAND_DIAGNOSIS.md for timing and ACK
ambiguity limits. This adds scoped changes to MavCommandQueue.h/.cc; it does not
modify FC/Herelink configuration or joystick assignments.

UDP command diagnostics in src/Comms/UDPLink.cc log command bytes, local endpoint,
target counts, each actual destination and the writeDatagram return value.
Only COMMAND_LONG, COMMAND_INT and SET_MODE envelopes are traced, excluding the
high-rate joystick stream. Comms.UDPLink is enabled by the diagnostic launcher.
Successful socket writes mean local acceptance, not delivery to the flight
controller. Socket error fields may retain an earlier error; interpret them
alongside the current write return value. Routing, sends and retry rules are unchanged.

The 2026-11-05 confirmation fix is a narrowly scoped upstream exception in
src/Vehicle/MavCommandQueue.cc: COMMAND_LONG uses the incremented queue counter
to send confirmation 0 on its first attempt, then 1, 2 on existing retries.
Retry eligibility, timeouts, command parameters and joystick dispatch are unchanged.
Earlier statements about unchanged upstream files describe the preceding baseline.

Custom OSD v1 architecture, sources, validity rules and bench verification:
[OSD.md](OSD.md).

The OSD reads its callsign from the flight controller SD root /callsign.txt
through MAVLink FTP while disarmed. Voltage uses the user-confirmed 12S profile
and shows V/cell. Camera indication uses Relay2 (index 1), OFF → CAM1 - MAIN,
ON → CAM2 - STAB. Feedback versus last-command provenance remains internal.
The OSD settings button and popup have been removed. Throttle %
uses the standard vehicle telemetry Fact; it does not represent individual RPM.
These additions do not change joystick mappings or send relay-switch commands.

Windows Debug overlay for the v5.1.5 baseline. The executable
target is HerelinkGCS; the runtime application name is Herelink GCS.
A minimal core plugin uses the official custom-example URL interceptor
architecture for the toolstrip and OSD resource overrides. The toolstrip
override omits Takeoff and Return from the
primary toolstrip. Land retains the standard action and confirmation, with
visibility based on showLand even when showTakeoff is true. All other actions,
the readiness panel, ARM/DISARM UI, flight mode indicator, and firmware
factories use the standard implementation. There are no settings overrides.

CH5 interlock and CH6 startup synchronization have not been implemented in
this checkout. MODE_SYNC_PROPOSAL.md remains a proposal; automatic startup
Land blocking is not active. Manual CH6 mappings, CH8 camera relay, and CH11
camera pitch paths are unchanged. This UI change neither adds nor removes
joystick ARM/DISARM mappings; remove any previously configured transmitter
ARM/DISARM button assignments manually in Joystick settings when migrating.

Takeoff and RTL remain available to the underlying vehicle/MAVLink APIs and
may appear in other standard contexts. Only primary Fly View entries change.

The official QGC_STABLE_BUILD option removes the Daily suffix and enables
upstream stable-release behavior. Settings use the existing QGroundControl
organization with a separate Herelink GCS application namespace; existing
QGroundControl Daily settings are not copied. Configure joystick, links, and
video in the custom application before hardware use.
Stable behavior also enables the upstream version check and omits translations
marked Test Only from the language selector. Debug pseudo-localization remains.

## Windows commands

For passive network diagnostics, run custom/capture-mavlink.ps1 from an
administrator PowerShell window. It captures only UDP 14552 to/from
192.168.144.11 into a 128 MB circular ETL under build-herelink-debug/captures.
Capture covers all Windows networking components so a NIC-only visibility gap
cannot be mistaken for an application send failure. The same IP/UDP filter applies.
Press Enter after an incident to stop and convert it to herelink.pcapng.
The script refuses existing PktMon sessions/filters it cannot safely exclude,
and removes only its own filter. It sends no vehicle commands and installs no software.

The run command enables command diagnostics via QGC's --logging option and
writes a unique session folder under build-herelink-debug/logs. commands.log
captures application output including relay/mode sends, ACKs and command queue
results; stdout.log captures standard output, and session.json records launch
metadata. latest-session.txt points to the latest session folder. Existing logs
are preserved. The launcher refuses a second instance: safely close the running
GCS before restarting to enable logging. No build or tests run for the run command.
QT_LOGGING_RULES alone is insufficient because QGC installs its own category filter.

Run from the repository root in PowerShell:

```powershell
.\custom\build-debug.cmd
.\custom\build-debug.cmd test
.\custom\build-debug.cmd ui-test
.\custom\build-debug.cmd run
```

The script pins MSVC 14.44.35207 through vcvarsall, Qt 6.11.1, and GStreamer
1.28.4 at their existing installation paths. It configures Ninja Debug with
tests and GStreamer enabled in build-herelink-debug. The executable is
build-herelink-debug/Debug/HerelinkGCS.exe. The test command enables verbose
test output and stderr logging for actionable failure diagnostics.

On 2026-11-05, configuring the mode-menu build initially failed because
.venv/pyvenv.cfg referenced a removed user-level Python 3.10.20 installation.
The same version was restored inside the ignored project environment using:

```powershell
uv python install 3.10.20 --install-dir .venv\.base-python --no-bin
```

The environment's home entry now points to
.venv/.base-python/cpython-3.10.20-windows-x86_64-none (absolute project path).
Its original configuration is saved as .venv/pyvenv.cfg.before-path-repair;
existing environment packages were preserved. No system Python was installed.
The subsequent custom/build-debug.cmd build succeeded. Build output is recorded
in build-herelink-flight-mode-menu.log; no tests were run.

The ui-test command runs the custom integration suite on the native Windows
display. It verifies the actual loaded toolstrip model, Land eligibility,
the mode label, and the readiness panel's Arm/Disarm presentation using a mock
ArduCopter. It never triggers an action or arms a vehicle. Qt 6.11's offscreen
backend asserted during full-window layout here, so this suite uses --onscreen.
The existing Unit suite keeps its original offscreen configuration.

## Existing upstream Unit test baseline

The unmodified Debug build passed 191 of 197 Unit-labelled tests. Existing
issues: GeoTagControllerTest (drive-letter case), BluetoothWorkerTest
(unexpected Windows Bluetooth warnings), QGCArchiveModelTest (local file URL),
QGCCompressionTest (symlink and Unicode paths), JsonResourceAuditTest (60-second
timeout), and PlatformTest (unit-test environment variable).

## Verified custom build result (2026-11-01)

The complete Ninja Debug build succeeded with MSVC 19.44.35229 from toolset
14.44.35207, Qt 6.11.1, and GStreamer 1.28.4. HerelinkGCS.exe reports file
version 5.1.5.0. The normal GUI launched with window title Herelink GCS.
The full Unit run passed 191/197 in 433.69 seconds, with exactly the six
baseline failures above and no new failing suites. Hardware RTSP playback,
telemetry, and joystick control have not been exercised in this custom build.
The just/pre-commit lint gate could not run because those tools are absent
from the local environment. No upstream tracked files were changed.

Build and test logs are at repository-root build-herelink-debug.log and
build-herelink-tests.log; detailed CTest output is in
build-herelink-debug/Testing/Temporary/LastTest.log.

## Primary Fly View controls verification (2026-11-01)

The UI update builds successfully with the same toolchain. The full Unit
comparison remains 191/197 passing, with exactly the same six baseline issues
and no new failing Unit suites (414.84 seconds). The custom HerelinkFlyViewTest
integration suite passes on the native Windows display. Existing camera zoom
and camera capture UI suites also pass using mock vehicles and --onscreen.
No real ARM commands or physical vehicle actions were issued.

The custom test checks the actual loaded QML model for absent Takeoff/RTL,
Land eligibility even when Takeoff would be available, the visible flight-mode
label, and the readiness panel's Arm and Disarm presentation. Physical CH6,
CH8, CH11, Herelink telemetry and RTSP playback still require a bench check.
No joystick assignments, calibration, FC parameters or failsafes were changed.

For a manual check, use a disarmed vehicle with propellers removed. Confirm the
primary toolstrip has no Takeoff/Return, open the readiness panel to inspect
Arm without activating it, and verify the mode label and warnings. Check the
existing CH6 positions, CH8 relay, CH11 pitch and RTSP feed under the established
bench procedure. Startup mode synchronization remains unimplemented; do not
assume an initial Land position is blocked by this UI change.

Logs: build-herelink-ui-final.log, build-herelink-ui-unit.log, and
build-herelink-ui-focused.log in the repository root. Camera test logs are in
build-herelink-debug/FlyViewCamera{Zoom,Capture}UITest.{stdout,stderr}.log.

Once this overlay exists, explicitly pass -DQGC_CUSTOM_DIR=custom-disabled
when configuring a future upstream-only build. The existing successful
upstream build and executable are preserved; do not reconfigure that directory
with the default custom-directory option.
