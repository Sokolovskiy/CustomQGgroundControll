# Camera tilt OSD

The read-only camera tilt dial sits beside the stick displays in the lower-left card.
The selected camera badge replaces its title, with the commanded angle below the
dial. The 3D attitude and compact telemetry occupy a separate lower-right card.
It shows a **commanded** camera angle, not measured servo feedback. The readout
contains only degrees (or a dash when unavailable), without a CMD suffix.
The user requested inversion after reviewing the indicator: PWM 1000 = down
(90 degrees), PWM 2000 = horizontal (0 degrees).

The current TX16S profile maps the camera control to USB axis 4 (zero based),
Additional Axis 6, using RC override. QGC sends this auxiliary axis as **RC10**;
the transmitter's CH11 name does not mean SERVO11 or MAVLink RC11.
The indicator passively observes existing outgoing RC_CHANNELS_OVERRIDE traffic
on the active vehicle's primary link and maps its calibrated RC10 command
1000..2000 to 90..0 degrees, preserving the existing control calibration and reversal.
There are no new commands, stream requests, parameter writes or axis remappings.

The indicator shows a dash until a valid command is observed. It becomes unknown
on link/vehicle/joystick changes, override release, loss of connection or stale
input (one second for the outgoing command and the existing 300 ms stick-input
freshness gate). It also requires an enabled, calibrated joystick and the existing
Additional Axis 6 RC-override configuration. It does not use gimbal P telemetry.

The lower-left card includes the sticks and camera dial. The warning banner stays
centered, moving right only if needed to clear that card. The existing 3D hide/show option
does not hide the camera tilt dial.

The former Tilt/Motion text legend is replaced with Alt, Climb and Throttle
beneath Roll/Pitch, all using the same 14-reference-pixel text. The timer sits
below the sticks in the lower-left card. This combined 3D/telemetry
card is anchored at the lower right. These readouts remain visible when 3D is disabled;
the 3D tilt and motion arrows and all underlying telemetry are unchanged.

Build: `custom\build-debug.cmd`. Launch: `custom\build-debug.cmd run`.
Automated tests are not run, as requested. Physical angle accuracy and direction
must be checked against the actual camera; the indicator cannot detect a stalled
servo or a command that did not reach the flight controller.

2026-11-05: Debug build succeeded (`build-herelink-camera-tilt.log`) and the
application was launched. Read-only window inspection confirmed the dial and
non-overlapping card/banner layout while disconnected. No CameraTilt QML errors
were found in this launch log. Live CH11 endpoint/direction verification is
pending; no flight or camera commands were issued for this inspection.
