# Custom 3D attitude indicator

Initially implemented as source changes only on 2026-11-05, as requested.
The user subsequently authorized builds and safely closed the application.
custom/build-debug.cmd completed successfully (exit code 0), and the updated
build-herelink-debug/Debug/HerelinkGCS.exe was launched through the normal
diagnostic launcher. Build log: build-herelink-attitude-3d.log. Startup session:
build-herelink-debug/logs/2026-11-05_16-46-39-982_98d3e4cd.

No QML loading errors were found in that startup log. Read-only window
inspection showed Disconnected / NO VEHICLE, so the attitude scene was
correctly gated off. The user subsequently supplied a screenshot confirming
the initial live model renders. Directions and GPU/video performance remain
unverified. No automated tests,
simulations, ARM/DISARM or other physical control commands were issued.

The subsequent double-size translucent panel, fixed horizontal reference and
roll/pitch readouts also built successfully. Build log:
build-herelink-attitude-size-reference.log; startup session:
build-herelink-debug/logs/2026-11-05_16-55-46-208_a3eee50a.
No QML errors were found at startup. The first inspection was disconnected.
After the user reported the panel missing with live telemetry, the strict layout
boundary check was corrected to allow one logical pixel of rounding tolerance.
At fractional viewport scaling, clamping to the bottom and then adding the card
height/margin could exceed that same limit by floating-point roundoff. No stale
telemetry or obstruction guards were removed. Visibility transitions now log
the precise gating reason and geometry, without continuous per-frame logging.

The fix built successfully (build-herelink-attitude-layout-fix.log) and was
launched with session logs/2026-11-05_17-01-12-123_99543ddc. The log recorded
visible at viewport 1527x730. Read-only window inspection confirmed the enlarged
3D model, translucent gold-bordered panel, fixed horizontal outline and Roll /
Pitch readouts rendered alongside live video and telemetry. No physical control
commands or automated tests were issued. Physical rotation directions and
performance under longer operation remain for the user's manual verification.

## Architecture and dependencies

The latest user-supplied OSD composition supersedes the earlier standalone
double-size card layout below. The 3D scene is embedded transparently in one
lower-left control card (390 x 132 reference units): camera/sticks on the left,
scene in the middle (140 x 116), and separate roll/pitch/motion readouts on the
right. Embedded mode suppresses its own frame and bottom caption. Relative
attitude, motor-down arrows, the one-degree deadband and cyan velocity arrow
are unchanged. PiP overlap detection now maps into the nested loader's coordinates.

The full overlay scales from the user's 1148 x 646 composition. ARM/mode share
a horizontal top-left card, callsign is top-center, OSD toggle is top-right
with room for native photo/video controls, battery sits above the control card,
the warning banner is bottom-center and flight metrics are bottom-right.
The existing video/no-video background and native toolbar remain intact.
No blue placeholder video or synthetic telemetry from the mockup is added.
Battery coloring applies only to the voltage digits; units and amperes stay white.
Build log: build-herelink-osd-reference-layout.log. The app was built and launched;
read-only inspection confirmed the layout while disconnected. Live data and
longer operation still require observation. No automated tests were run.

DroneAttitude3D.qml is a reusable, passive OsdPanel with a required quaternion
attitude property, plus read-only roll/pitch inputs. The panel uses the same
translucent background and accent border as the other OSD cards. Its transparent,
offscreen Qt Quick 3D scene contains twelve aircraft
primitive Models: two X arms, the body, a nose marker and four motor/propeller
pairs. Gold (#ce9800) front motors and a nose marker distinguish the front.
The fixed three-quarter camera looks slightly downward from the rear/right.
The user requested a double-size model and a clearer horizontal reference.
A fixed four-edge horizontal outline at scene y=-28 stays outside the aircraft's
rotation node. Roll and pitch readouts below the scene use Vehicle roll/pitch
Facts' raw values (degrees), including one decimal place. Zero indicates level;
positive roll is right side down and positive pitch is nose up. This is attitude
relative to the estimator's level reference, not clearance above terrain.
There are no geographic coordinates, terrain, rotor animations,
reflection probes, shadows, smoothing animations or input handlers in the scene.
One directional light and three shared, rough nonmetallic materials are used.

QtQuick3D is already a required Qt component in the root CMakeLists.txt and
Qt6::Quick3D is already linked by src/CMakeLists.txt. The existing Qt 6.11.1
installation contains the QML types used here. No new packages, versions,
downloads, external assets or CMake dependency changes are introduced. Only
the new QML component is added to custom/custom.qrc.

The existing DroneModelDjiF450.qml was inspected. It imports the geographic
Viewer3D module, instantiates Viewer3DGeoCoordinateType, applies coordinates and
altitude, uses intermediate mesh transforms and animates propellers based on
armed/flying state. Its imported mesh assembly would require a separate static
wrapper and axis/scale calibration. Simple primitives avoid those dependencies
and make body axes explicit for this compact attitude-only view.

## Telemetry and coordinate conventions

HerelinkOsd's existing subscription to the active Vehicle's
mavlinkMessageReceived signal is reused. Vehicle.cc emits this signal after its
normal Fact and firmware processing. There is no additional link, MAVLink
parser, stream request, active-vehicle subscription or timer for this feature.
No control commands are generated.

VehicleFactGroup exposes roll/pitch/heading Facts in degrees, but truncates
heading to whole degrees and does not expose its quaternion. The widget uses
the supported Vehicle message path to preserve full yaw precision and avoid
converting quaternion telemetry back through Euler angles:

- ATTITUDE: finite roll/pitch/yaw in radians are converted to degrees for
  QQuaternion::fromAxisAndAngle. Composition is qZ(yaw) * qY(pitch) * qX(roll),
  the MAVLink intrinsic Z-Y-X convention, not Qt's fromEulerAngles convention.
- ATTITUDE_QUATERNION: q1..q4 are (w,x,y,z). The representation offset is
  right-multiplied when supported, following VehicleFactGroup's convention.
  Finite, reasonably unit-length quaternions are normalized; invalid samples
  are unavailable. A valid quaternion stream takes precedence over Euler
  ATTITUDE, matching the upstream source preference for that connection.

The native attitude maps a forward/right/down body into the NED attitude
reference. The first valid, nonvertical forward direction supplies a yaw-only
reference q0. Removing q0 on the left preserves roll and pitch:

    qRelative = conjugate(q0) * qNative

Scene/model axes are +X right, +Y up and -Z forward. The basis quaternion
C=(0.5,0.5,0.5,-0.5), in (w,x,y,z) order, maps (N,E,D) to (E,-D,-N):

    qScene = C * qRelative * conjugate(C)

The same basis conversion is applied to body and reference. Positive native
roll lowers the right side; positive pitch raises the nose; positive yaw turns
the nose right about the downward native axis. The fixed oblique camera affects
screen projection but never follows vehicle yaw. The quaternion path avoids an
Euler singularity in the display rotation. The Euler-only fallback remains
limited by the Euler telemetry representation supplied by the FC.

Yaw is relative, not an absolute compass heading. It may drift without a
compass and is re-zeroed on vehicle/link change, disconnect, invalid/stale
attitude recovery or the first switch to quaternion telemetry. At initial
vertical-nose attitude, a horizontal yaw reference is undefined; the widget
waits for a nonvertical sample. GPS, heading Facts and position are not required.

## Freshness, rendering and placement

The existing 100 ms OSD refresh publishes attitude at at most 10 Hz. Incoming
samples are retained without adding a render timer or interpolation latency.
An attitude sample older than 1000 ms is unavailable at the next refresh (up to
100 ms later). Existing heartbeat/communication-loss checks also gate validity.
Freshness is local receive freshness, not a synchronized FC sample-age estimate.
Invalid quaternions, vehicle/link switching and disconnect reset the display
reference. Quaternion precedence means loss of that stream hides the widget
even if lower-priority Euler messages continue; reconnect establishes a new
source preference. No frozen model remains presented as live after timeout.

A Loader under the existing OSD graphics group creates the scene only while
the overlay, video view, setting and fresh attitude are available and the layout
has space. Disabling/hiding it or losing telemetry destroys the View3D and its
offscreen scene resources through normal Qt ownership. The render target is
capped at 880x880 pixels after the size increase; no AA, extra render loop or continuous animation is
requested. Actual GPU cost still needs measurement with RTSP video running.

The widget is beside the stick panel in the lower left. Its normal size is
about 400 px, capped at 440 px and reduced to 240 px on small viewports, twice
the previous scene dimensions. A separate bottom strip contains the readouts. Layout
checks keep it left of the central reticle and callsign and clear of the
telemetry panel and viewport bottom; it hides when there is insufficient space.
Its vertical position is clamped upward to accommodate the larger card.
It also hides while native virtual joysticks are enabled, or when its rectangle
intersects the expanded PiP content. PiP uses the existing
mapControl.pipView._pipContentItem alias: the outer PiP size remains nonzero
while collapsed, so generic corner insets would incorrectly hide the model.
This alias is a v5.1.5 implementation detail to recheck during upstream upgrades.
Both the Loader and scene are disabled for input. Existing cards and video
interaction are not moved or covered by a mouse interceptor.

## Visibility setting

Right-click or hold the existing small OSD button to open its context menu.
The keyboard Menu key also opens it when the button has focus. Toggle
**Show 3D Attitude** there. It defaults to enabled and is stored through the
existing custom QSettings approach at HerelinkOSD/show3DAttitude in the
application settings. This is an application-only preference, not an FC Fact
or parameter. No new settings button is introduced. A normal left click keeps
the original whole-OSD toggle behavior. Turning the entire OSD off also destroys
the 3D scene; the separate preference is retained.

## Tilt and measured-motion arrows

AttitudeArrow3D.qml renders a stationary shaft/cone along local -Z, rotated by
a backend quaternion. Both arrows are outside the aircraft's rotation node:

- Gold Tilt: individual downward arrows beside the motors on the lowered side,
  replacing the earlier single horizontal lean arrow. Total inclination up to
  and including one degree is ignored. Motor offsets (+/-58 on model X/Z) are
  rotated with the same scene quaternion as the aircraft. Only motors below
  the center's horizontal plane are marked (0.05 scene-unit height tolerance).
  Markers are displaced radially outward from each motor and point along fixed
  scene -Y, independently of body rotation. A pure bank/pitch may mark two
  motors; a diagonal lean may mark one. No individual rotor thrust is inferred.
- Cyan Motion: the FC-reported velocity estimate, transformed with the same
  yaw-reference removal and NED-to-scene basis as the model. It can point
  vertically when a vertical velocity estimate is available.

The existing active-Vehicle message subscription records timestamps for
LOCAL_POSITION_NED and ESTIMATOR_STATUS or ArduPilot EKF_STATUS_REPORT. Velocity
values come from VehicleLocalPositionFactGroup's vx/vy/vz raw Facts in m/s.
When local vz is unavailable, fresh VFR_HUD-backed climbRate is used as a
vertical fallback (NED vz = -climbRate). Horizontal velocity requires finite
fresh local vx/vy plus attitude/horizontal-velocity validity flags and exclusion
of constant-position mode. Vertical velocity requires its corresponding
estimator validity flag and finite fresh velocity. Estimator status expires
after 3000 ms; local velocity and the climb fallback expire after 1500 ms.

Only the available axes contribute to the displayed speed and vector; the
label explicitly says Motion XY, Motion Z or Motion XYZ. Missing estimates
show Motion —, not zero or a synthetic vector. Without GPS, optical flow/VIO
or another usable horizontal-velocity estimate, horizontal motion cannot be
shown. FC validity flags are trusted as reported; this cannot independently
verify estimator accuracy. Local velocity and attitude must share the FC's
NED reference. No course-over-ground or absolute compass heading is assumed.

The numeric speed remains visible near zero, but its arrow is suppressed below
0.2 m/s to avoid direction jitter. Arrow lengths are fixed and do not represent
speed magnitude. Roll/pitch numeric readouts additionally show left/right-bank
and nose-up/down glyphs outside an inclusive one-degree deadband, in body-axis terms.
These glyphs are not motion directions. The bottom strip is taller to fit the
two rows. Existing loader geometry tolerance and telemetry-loss gates remain.

No new stream requests, links, timers, FC parameters or control commands are
introduced. Build log for this addition: build-herelink-attitude-arrows.log.
Automated tests and physical maneuvers are not performed.
The Debug build completed successfully and was launched in diagnostic session
logs/2026-11-05_17-32-09-937_1ea6b1ca. Read-only window inspection confirmed
the gold scene arrow and directional roll/pitch readouts rendered. Motion —
was displayed while no usable motion estimate was available. No QML loading
errors were found. Actual motion-arrow direction and estimator availability
under movement remain unverified; no firmware update or flight command was
performed. The existing upstream firmware-version notification was left intact.

The per-motor Tilt revision uses build-herelink-motor-tilt-arrows.log. The cyan
Motion arrow, its velocity sources, validity checks and thresholds are unchanged.

## Files for this change

Created:

- custom/qml/DroneAttitude3D.qml
- custom/qml/AttitudeArrow3D.qml
- custom/ATTITUDE_3D.md

Modified:

- custom/src/HerelinkOsd.h
- custom/src/HerelinkOsd.cc
- custom/qml/HerelinkOsdView.qml
- custom/qml/FlyViewCustomLayer.qml
- custom/custom.qrc
- custom/OSD.md
- custom/README.md

Earlier command-queue/UDP diagnostic changes remain in the working tree;
no upstream source is edited for this feature. ARM/DISARM, modes, relays,
joystick/RC handling, video decoding, FC parameters and MAX7456 configuration
are unchanged by this addition.

## Deferred manual verification

The implementation was source-reviewed against the checked-out QGC, MAVLink
definitions and installed Qt type/header declarations, and subsequently built
successfully. Startup QML loaded without reported errors. Materials, live 3D
scene instantiation, rotation projection and performance are still unverified.

1. Build manually using custom/build-debug.cmd when ready, then launch normally.
   Confirm the component imports and there are no QML or rendering warnings.
2. With the aircraft kept disarmed, verify right/left roll, nose-up/down pitch,
   relative yaw and combined rotations against physical orientation. Check yaw
   continuity through 359/0 and quaternion behavior near vertical/inverted poses.
3. Confirm model scale, front/rear distinction, transparent composition and
   video frame rate/latency at the actual Windows scaling and viewport sizes.
4. Check the OSD button's ordinary click, hold, right-click and keyboard menu;
   confirm the independent preference persists after a normal restart.
5. Verify scene removal on OSD-off, map view, stale attitude with continuing
   heartbeat, disconnect and active-vehicle switch. Verify reacquisition does
   not reuse the preceding vehicle's orientation or yaw reference.
6. Expand/resize/collapse PiP, resize the window and check native controls and
   virtual joystick visibility. Confirm clicks over the model reach the usual
   video interaction and that no control behavior changed.

Only the build, application launch and disconnected-window inspection have
been performed so far. Tests, SITL, hardware commands and device modifications
have not been performed to execute this checklist.
