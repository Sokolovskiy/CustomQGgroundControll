# Herelink GCS OSD v1

The custom OSD is an application overlay over main Fly View video. The supplied
reference image is not bundled. The existing video decoder, transport, joystick
dispatch, flight modes, ARM/DISARM, FC parameters and failsafes are untouched.
The previous primary toolstrip policy is retained. CH6 startup synchronization
is still only a proposal; this feature does not implement or change it.

## Architecture and settings

HerelinkCorePlugin redirects FlyViewCustomLayer.qml and the small bottom-right
instrument row, alongside the previous toolstrip override. It does not replace
FlyView.qml. HerelinkOsd provides read-only presentation data through a QML
context property. HerelinkOsdView, OsdPanel and OsdStick separate presentation.

The bottom-right override hides only duplicate telemetry and instruments while
custom OSD is visible on main video. Map mode and OSD-off retain native controls.
There is no supported option for hiding the telemetry bar alone, hence this
small row resource override instead of a full Fly View replacement. Safety
warnings, readiness, preflight checks, guided actions, toolbar and camera UI
remain upstream. OSD cards anchor to the viewport edges, below the main toolbar,
matching the reference. Corner-widget insets are passed through for the native
map layout but are not applied as full-edge OSD margins: collapsed PiP and hidden
panels can report large insets even when their content is not displayed. If a
native corner control is expanded it can share that corner; the passive OSD
graphics do not consume its input. Custom graphics have no
MouseAreas or input handlers. Only OSD/settings buttons and the settings popup
accept input. The OSD toggle stays available when graphics are off.

The OSD settings button and popup are removed. Overlay settings use QSettings
under HerelinkOSD in Herelink GCS.ini, not FC parameters. Callsign comes only
from FC SD /callsign.txt through the shared
MAVLink FTPManager; it is not an editable local setting. Before a successful
read it is empty, shown as CALLSIGN —. Visibility defaults
to on. The user confirmed a 12S battery: cell count defaults to 12, including
before a UID is available; unknown/invalid legacy counts fall back to 12.
Explicit positive per-UID cell-count overrides remain supported internally.

The user-specified camera hardware profile is relay index 1 (Relay2), OFF selects
CAM1 and ON selects CAM2. It is documented but not live-verified. Per-UID settings
may override or disable this mapping; state always starts unknown. This page
never changes RELAY2_PIN, RELAY2_FUNCTION or any other FC parameter.

## Real data sources and validity

The outer side margins are 8.5 reference units, top margins 10, and bottom
margins 12.5. The flight mode card at the top left uses
translation-aware ArduCopter mode names: Altitude Hold -> black, Position Hold
-> #ce9800, Land -> #69ff37; other modes or disconnected state stay neutral.
Light backgrounds use black mode text. No mode is selected or inferred by this
styling. The arming badge is above the 3D model in the bottom-right card:
DISARMED uses black text on white, ARMED uses white text on red, and NO VEHICLE
uses white text on the neutral OSD background.

FlyViewTopRightColumnLayout.qml is overridden to omit PhotoVideoControl (native
record/photo/settings strip). TerrainProgress is retained and positioned below
the OSD button when needed. The OSD toggle's former camera-strip reserve is
removed so it can sit near the right edge. Only this UI entry point is removed;
camera manager, RTSP/decoding, recording backend, CH8 and CH11 are untouched.
Changes are confined to custom resources/interception; no upstream source is
edited for this update. Build log: build-herelink-osd-edge-mode-colors.log.
The Debug build succeeded and was launched in session
logs/2026-11-05_18-57-27-328_f1dade8d. No QML loading errors were found.
Read-only window inspection with live telemetry confirmed reduced edge margins,
the absent native camera strip, and a black Altitude Hold card (which still
included the arming badge at the time).
Position Hold/Land colors were source-reviewed without issuing mode commands.
No automated tests or hardware control actions were performed.

The standalone 3D attitude scene, coordinate conversion, freshness policy and
independent Show 3D Attitude preference are documented in
[ATTITUDE_3D.md](ATTITUDE_3D.md). Right-click or hold the existing OSD button
for that context menu; no settings gear is restored. Its default is enabled.
The scene shares the existing backend's active-Vehicle subscription and 100 ms
refresh. It is destroyed when hidden or telemetry is unavailable. This addition
was subsequently built and launched after the user authorized builds. Startup
QML loaded without reported errors; live rendering remains to be verified with
fresh attitude telemetry. Automated tests were not run.

| Readout | Source | Unavailable policy |
| --- | --- | --- |
| Armed / mode | Active Vehicle.armed / flightMode after autopilot HEARTBEAT | NO VEHICLE / — before first heartbeat, on communication loss, or after 3 s heartbeat silence |
| Status banner | Vehicle.prearmError, warning-or-higher STATUSTEXT, critical/emergency HEARTBEAT state, HealthAndArmingCheckReport | No unconditional OK. OK requires supported report, canArm, no report warnings/errors, disarmed and initial connection complete |
| Voltage | BatteryFactGroup id 0 voltage.rawValue divided by the user-confirmed 12S count (or explicit per-UID override) | V/cell only. — without finite positive voltage, fresh battery status or active connection |
| Current A | BatteryFactGroup id 0 current.rawValue | — on NaN/unknown current or battery status older than 5 s. Never uses mahConsumed |
| Relative altitude m | Vehicle.altitudeRelative.rawValue, updated by GLOBAL_POSITION_INT or ALTITUDE | — until a corresponding valid message, after 3 s silence, or disconnected |
| Climb m/s | Vehicle.climbRate.rawValue, updated by VFR_HUD | — until finite VFR_HUD.climb, after 3 s silence, or disconnected |
| Throttle % | Vehicle.throttlePct.rawValue, updated by VFR_HUD | — before a fresh message, after 3 s silence, outside 0–100 or disconnected. Overall reported throttle, not individual motor RPM |
| Callsign | FC SD /callsign.txt via MAVLink FTP | CALLSIGN — until a successful read; first UTF-8 line, trimmed, at most 64 display characters; reject invalid text and files above 4096 bytes |
| Stick monitors | Active Joystick.axisValues(roll,pitch,yaw,throttle) | Inactive until fresh (300 ms), enabled, calibrated, running normal polling for active connected vehicle |
| Camera | Incoming RELAY_STATUS present/on bitmasks, plus passive outgoing targeted COMMAND_LONG / DO_SET_RELAY observation | CAM — on startup/disconnect/link change until established. Display CAM1 - MAIN / CAM2 - STAB. Internally FC feedback expires after 15 s; CMD is last observed command, not physical feedback or ACK |
| Timer | Monotonic local elapsed time while fresh telemetry says ARMED | — before observing an armed session or after disconnect; not an FC flight-time counter |

Facts are not assumed valid merely because they contain an initial zero.
Only the numeric voltage in the battery readout uses unrounded, fresh per-cell voltage for its text color:
below 3.4 V is red, 3.4 through 3.6 V inclusive is yellow, and above 3.6 V is
white. Unknown/stale voltage keeps the unavailable readout white. These are
presentation thresholds only; no FC battery parameters, warnings or failsafes
are changed. Numeric display remains rounded to two decimal places.
The battery icon, V/cell unit, separator and current in amperes remain white.

Message freshness is tracked independently per field. All message filters target
the active autopilot component. Inactive vehicles are not displayed. The battery
selection is explicitly id 0; additional batteries are not silently combined.
GLOBAL_POSITION_INT refreshes altitude only when its value matches the Fact:
QGC's sticky ALTITUDE precedence must not make an older, different altitude
look fresh because unrelated position messages continue arriving.

After initial connection completes and a two-second settling period, the overlay
requests RELAY_STATUS once through Vehicle::requestMessage. It polls every five
seconds only after valid configured-relay feedback proves support. Unsupported
firmware leaves CAM — until an existing CH8 command is observed; it never assumes
CAM1 at startup. OFF/ON mapping remains Relay2 index 1 → CAM1/CAM2. No relay write
is sent for discovery, and reported relay state cannot prove which physical
camera produces the image.

The SD read starts only while disarmed. The shared FTP manager refuses a busy
transfer; the overlay retries for up to 30 seconds without cancelling another
client's operation. It reads once per connection; reconnect to retry after a
missing file or failure. Status/reload remain backend APIs without an OSD editor. Vehicle change
or lost telemetry clears the callsign, preventing a previous vehicle's name from
being displayed. Local temporary files are used only for the download; the FC
file is never written. Old local callsign settings are ignored. These metadata
requests add read traffic, but do not alter MAVLink transport or control logic.

Warning STATUSTEXT is supplemental and displayed for 12 seconds (individual
MAVLink chunks may be truncated); core QGC notifications retain their complete
message handling. A lower-severity text cannot replace a more serious text
within that window. Critical/emergency system state has highest custom priority;
error-or-higher text precedes PreArm errors, which precede ordinary warnings.
Stale or disconnected data never produces a green PreArm OK banner. The custom
OSD never hides core warnings, including legitimate no-GPS/prearm warnings.
Connected, disarmed vehicles without a confirmed positive arming report or an
actual warning show no custom banner; unavailable status is not treated as OK.

axisValues already contains QGC calibration, reversal, deadband and configured
response shaping. L uses yaw/throttle; R uses roll/pitch. Throttle [0,1] is mapped
to [-1,1], except center-zero vehicles where the existing centered value is used.
Samples are timestamped on the emitting thread and queued by value to the GUI;
joystick generations reject queued data from a previous device/enable cycle.
No configuration polling, synthetic RC channels, SDL polling or joystick command
dispatch hooks are added. Verify physical Mode 2 direction during bench testing.

## Timer policy

Start at zero on the first observed armed state; freeze the displayed duration
on disarm. Reset on the next arm. Vehicle switch, link/heartbeat loss, application
restart and reconnect invalidate/reset it. If connected to an already armed
vehicle, measure from observation, never claim its earlier airborne time.
No armed time is reconstructed or counted across a telemetry gap.

## Commands and validation

```powershell
.\custom\build-debug.cmd
.\custom\build-debug.cmd ui-test
.\custom\build-debug.cmd test
.\custom\build-debug.cmd run
```

HerelinkOsdTest covers disconnected/no-data state, settings persistence, explicit
cell-count calculation, pack-voltage fallback, stale telemetry/throttle, timer
resets, passive camera command decoding (including split frames), relay feedback,
callsign decoding/shared FTP completion isolation and joystick generation/disabled data.
HerelinkFlyViewTest adds presentation screenshots at 1920x1080, 1280x720 and
960x640, plus show/hide checks. Screenshots are disconnected presentation
fixtures on charcoal, not evidence of live Herelink video or flight telemetry.
They are written to build-herelink-debug/osd-screenshots/.

Use native Windows display for full QML integration tests; the existing offscreen
Qt backend has a known full-window layout assertion. Unit tests retain the
upstream configuration. Compare results with six known upstream failures in
README.md; new test suites are counted separately.

Hardware-dependent acceptance remains manual: real RTSP continuity, TX16S
neutral/extreme direction, physical camera selection, battery sensor accuracy
and actual failsafe/PreArm messages cannot be established with screenshots.

## DISARMED bench procedure (propellers removed)

1. Start with video as the main Fly View pane. Check original warnings and
   toolstrip behavior. Do not arm or activate any flight action.
2. Observe no-data dashes before connection. Connect telemetry and verify mode,
   arm state and valid altitude/climb against standard QGC telemetry.
3. Verify battery 0 voltage labelled V/cell equals total divided by 12. Confirm
   current is A and Throttle % matches standard QGC telemetry while disarmed.
4. Place callsign.txt in the FC SD root using the established file workflow.
   Reconnect disarmed and compare the displayed name. Missing file/unsupported
   FTP leaves CALLSIGN —. No local callsign is substituted.
5. Move both TX16S sticks to neutral/extremes; verify Mode 2 orientation and
   calibrated reversal. Disable/unplug the joystick: monitors must go inactive.
   Preserve CH6/CH8/CH11 assignments and do not use any ARM mapping.
6. Camera indicator initially shows CAM —. If RELAY_STATUS is supported, confirm
   MAIN/STAB indication after the read query. Operate the existing CH8 switch only
   when safe; compare index 1 OFF/ON against the actual image.
   This is the required live mapping verification.
7. Toggle OSD off/on with mouse and keyboard. Video must continue, core warnings
   stay visible, native instruments return, and the toggle remains available.
8. Disconnect/reconnect telemetry and verify stale values disappear, camera
   state resets and timer is unknown. Armed timing is automated with synthetic
   state only; do not arm a physical vehicle for these checks.

Core QGC's dedicated video fullScreen mode hides FlyViewCustomLayer upstream;
use main-video Fly View for this version. Onboard MAX7456 OSD is unchanged.

## Changed source files

Created:

- custom/src/HerelinkOsd.h and HerelinkOsd.cc
- custom/qml/FlyViewCustomLayer.qml
- custom/qml/HerelinkOsdView.qml
- custom/qml/OsdPanel.qml and OsdStick.qml
- custom/qml/FlyViewBottomRightRowLayout.qml
- custom/test/HerelinkOsdTest.h and HerelinkOsdTest.cc
- custom/OSD.md

Updated:

- custom/CMakeLists.txt and custom/custom.qrc (sources, resources and tests)
- custom/src/HerelinkCorePlugin.h and HerelinkCorePlugin.cc (read-only context,
  two additional narrowly scoped URL overrides)
- custom/test/HerelinkFlyViewTest.h and HerelinkFlyViewTest.cc (OSD layout,
  toggle, screenshots and core PreArm warning retention)
- custom/README.md (OSD documentation link and current architecture)

No tracked upstream source files were changed. The upstream executable and
successful build directory are preserved. The reference PNG is not in resources.

## Verified results (2026-11-02)

- Windows Debug compilation and linking succeeded with the existing MSVC
  14.44.35207 / compiler 19.44.35229, Qt 6.11.1 and GStreamer 1.28.4.
- HerelinkOsdTest passes. The timer reconnect gap discovered by this test was
  corrected before the final run.
- HerelinkFlyViewTest passes, including the original toolstrip/readiness checks,
  OSD-off core PreArm warning, show/hide and all three screenshot sizes.
- Existing camera capture and camera zoom UI tests pass using mock vehicles.
- Full Unit comparison: 192/198 passed, comprising the previous 191/197 and
  the new passing OSD suite. The same six baseline suites fail; no new Unit
  failures in sequential runs. The initial sequential comparison took 412.25
  seconds. After the final warning-priority refinement, the sequential rerun
  again produced 192/198 with the same six baseline failures in 409.23 seconds.
- An eight-worker parallel rerun produced an additional SurveyComplexItemTest
  timeout at its existing 60-second limit. Survey passed alone in 26.52 seconds
  and passed again in the final full sequential run. This parallel-load timing
  sensitivity is recorded separately rather than hidden in the baseline.
- The first link attempt was blocked by the running executable (LNK1168).
  The user closed the GCS; subsequent linking succeeded. No process was killed.
- The final custom UI workflow command passed in 24.97 seconds. The normal
  application then launched as Herelink GCS and remained responsive after
  20 seconds. No custom-resource loading errors were found in the startup log.
  Live RTSP continuity, physical joystick orientation and relay hardware mapping
  remain DISARMED bench checks, not automated-test claims.

Logs in repository root: build-herelink-osd-final.log (final workflow build),
build-herelink-osd-incremental.log, build-herelink-osd-focused.log and
build-herelink-osd-unit.log. Final sequential results are in
build-herelink-osd-unit-final-serial.log; the parallel run is in
build-herelink-osd-unit-final.log and isolated Survey retest in
build-herelink-osd-survey-retest.log. Native camera test logs are under the build folder
with OSD-FlyViewCameraZoomUITest and OSD-FlyViewCameraCaptureUITest prefixes.
Final UI command results are in build-herelink-osd-ui-final.log. Normal startup
logs are build-herelink-debug/osd-launch.stdout.log and osd-launch.stderr.log.
The standard just/pre-commit lint tools are not installed; they were not added.

### Viewport edge correction (2026-11-02)

Removed full-edge OSD reserves derived from native corner-widget insets. The
real Fly View could report roughly 400 px for collapsed PiP, 360 px for a hidden
right panel and 240 px at the bottom, incorrectly shifting the entire OSD toward
the center. Native layout still receives its original insets; custom OSD uses
the viewport edges below the toolbar. Added an integration regression case with
these large insets at all three supported capture sizes. Debug build succeeded,
and HerelinkFlyViewTest passed in 26.04 seconds. Only presentation, its tests
and this document changed; telemetry and vehicle control code are untouched.

### SD callsign, voltage, camera state and throttle update (2026-11-02)

Changed files for this update:

- custom/src/HerelinkOsd.h and HerelinkOsd.cc: read-only SD download, relay
  status discovery, pack voltage fallback and standard throttle Fact display.
- custom/qml/HerelinkOsdView.qml: voltage unit, Throttle row and SD reload/status.
- custom/test/HerelinkOsdTest.cc: pack/stale voltage, throttle, relay status,
  callsign parser and isolated shared FTP completion regression coverage.
- custom/OSD.md and custom/README.md: current sources and acceptance procedure.

The full custom/build-debug.cmd workflow succeeded. After the final regression
test additions and FTP wait-window refinement, the incremental build linked
successfully. An overlapping UI test briefly blocked that incremental link with
LNK1168; repeating the link after the test exited resolved it. The latest EXE
then passed HerelinkFlyViewTest and HerelinkOsdTest in 28.56 seconds.
The final sequential Unit run passed 192/198 in 415.18 seconds, with exactly
the six upstream baseline failures listed in README.md and no new failing suites.
The just/pre-commit lint tools remain unavailable locally and were not installed.
The normal application launched with title Herelink GCS and remained responsive.
Startup logs contain no custom QML loading errors. Existing warnings recur for
QQuickPinchArea, Windows location permission, UDP multicast membership and the
unavailable GStreamer gio giolibproxy.dll module; the previous osd-launch.stderr.log
contains the same warnings. Live video continuity is not established by startup.

No real ARM, relay switch, motor, parameter-write or file-upload commands were
issued by the automated checks. Tests simulate FTP completion and relay status;
they do not establish live FC FTP compatibility, camera wiring or ESC RPM.
The existing upstream build and tracked sources remain unchanged.

### Custom no-video background (2026-11-02)

The supplied fatum.jpg is bundled unchanged as custom/resources/fatum.jpg.
HerelinkCorePlugin redirects only the QML image URL qrc:/res/NoVideoBackground.jpg
to qrc:/Herelink/fatum.jpg, registered in custom/custom.qrc. The upstream video
QML retains its aspect-preserving crop, WAITING FOR VIDEO / VIDEO DISABLED label
and visibility rules. Live video continues to replace the placeholder normally.
No video pipeline or upstream resource is modified.
The custom/build-debug.cmd Debug build succeeded, and the existing Fly View
integration suite passed through custom/build-debug.cmd ui-test in 26.76 seconds.
Logs: build-herelink-background.log and build-herelink-background-ui.log.
The application launched and remained responsive without image-loading errors.
background-launch.stderr.log repeats the existing GStreamer "Could not open
resource for reading and writing" error also present in both previous launch
logs. Live-stream availability was not verified by this background update.

Logs: build-herelink-osd-sources.log, build-herelink-osd-sources-incremental.log,
build-herelink-osd-sources-test.log and build-herelink-osd-sources-unit.log.
Normal startup logs: build-herelink-debug/osd-sources-launch.stdout.log and
osd-sources-launch.stderr.log.

### MAIN/STAB, 12S voltage and command diagnostics (2026-11-02)

The camera card now displays CAM1 - MAIN / CAM2 - STAB for the existing
Relay2 OFF/ON states; unknown remains CAM —. FC/CMD provenance is retained in
the model but is no longer appended to the visible camera name. This does not
turn a last-command observation into physical camera feedback.

The user confirmed 12 series cells. The default is 12 and invalid/zero legacy
counts use 12; the card always shows V/cell (44.21 V / 12 = 3.68 V displayed).
Existing positive per-UID overrides remain internal; this installation has none.
The OSD gear and settings popup are removed. Only the OSD toggle remains.

The reported "Vehicle did not respond to command: Set relay" is emitted by the
upstream MavCommandQueue after its retries receive no matching ACK. The existing
background-launch.stderr.log records DO_SET_RELAY timeouts and also timeouts for
SET_MESSAGE_INTERVAL and a RELAY_STATUS request. The external joystick action
JSON specifies command 181, relay index 1, values 0/1, default autopilot component
1; these mappings are unchanged.

Read-only analysis of saved telemetry 2026-11-02 12-30-32.tlog found four accepted
relay ACKs from FC sys/comp 1/1. Both that file and 13-39-06.tlog also contain
failed command ACKs from another sys/comp 43/250, alongside accepted mode ACKs
from 1/1. The source of the additional endpoint and the cause of the more recent
missing ACKs are not established; these earlier recordings do not prove the
cause of the current timeout. No blanket ACK acceptance, timeout increase,
warning suppression, joystick dispatch, routing or FC parameter change is made.

Herelink.ControlDiagnostics logs outgoing relay/mode COMMAND_LONG requests,
incoming autopilot ACKs and Vehicle::mavCommandResult (including timeouts and
duplicate failures). It observes existing traffic without generating control
commands. The next occurrence can be correlated using source/target IDs,
confirmation, params, ACK result and failure code in osd-controls-launch.stderr.log.
No automated tests are run for this update, as requested by the user.
The custom/build-debug.cmd build succeeded with the existing toolchain. The
application is launched with Vehicle.MavCommandQueue.debug enabled for this
session only; osd-controls-launch.stdout.log and osd-controls-launch.stderr.log
under build-herelink-debug capture its output. Build log:
build-herelink-osd-labels.log. All code changes remain inside custom/.
