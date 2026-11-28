# Windows TX16SMK3 control recovery

## Evidence and cause

The working logs (including 2026-11-19 14:14:06) contain button assignments
2/3/4 = Altitude Hold/Position Hold/Land and 5/6 = Camera Relay OFF/ON.
At investigation time, `%APPDATA%/QGroundControl/Herelink GCS.ini` retained
calibration and Additional Axis 6 but had no `JoystickButtonActionSettingsArray`
entries. `Calibrated` validates axes, not button assignments. The button loop
skips unassigned buttons; there is no independent custom raw-button RC handler.
Thus `No Action` disables these functions in this implementation.

Settings are keyed by SDL display name under `JoystickSettingsV2/<name>`, not
by USB serial number. Identically named radios share that profile when connected
one at a time. Simultaneous radios receive separate names (suffix `#2`, etc.) and
settings groups. Replacing hardware does not restore deleted settings.
The exact event that cleared the saved actions is not established by the logs.

Discovery also had reproducible defects: the suffixed display name was compared
to the unsuffixed hardware name, causing the second object's replacement on
rescan; a new instance could reuse a cached object solely by name, retaining old
capabilities/open handles; and the manager compared keys without detecting a
replacement object at the same key. These paths are now covered by regression
tests. Hardware name and display name are tracked separately; new instances
get fresh objects, and manager selection follows object replacement.

## Actual control paths

| Function | USB / UI input | Outgoing MAVLink |
| --- | --- | --- |
| Sticks | Calibrated four stick axes | `MANUAL_CONTROL`; auxiliary override ignores RC1–4 |
| ARM/normal DISARM | Existing readiness UI | Existing arm command |
| Emergency force-disarm (transmitter CH5) | Physical USB buttons **0/1** (low/high), independent of button actions; fresh low then high held 3 seconds while ARMED | One `MAV_CMD_COMPONENT_ARM_DISARM`, params `0, 21196`; no RC5 override |
| Modes (transmitter CH6 function) | Buttons **2/3/4**, zero based in both UI and settings | Firmware flight-mode action, normally `MAV_CMD_DO_SET_MODE`; no RC6 PWM |
| Cameras (transmitter CH8 function) | Buttons **5/6** | Verba: `MAV_CMD_DO_SET_RELAY`, Relay2/index 1, OFF/ON; Scorpion HDMI profile: `MAV_CMD_VIDEO_START_STREAMING`, input 0/1 |
| Tilt (transmitter CH11 label) | USB axis **4**, Additional Axis **6** | `RC_CHANNELS_OVERRIDE.chan10_raw`, MAVLink RC**10**, 1000–2000 |

Additional Axis N maps to MAVLink RC(4+N), not to the transmitter channel name
or the FC servo output number. Disabled RC5–8 carry release `0`; disabled
RC9–10 carry extension-field release `65534`; RC1–4 carry ignore `65535`.
RC11–18 are unused (`0`, which means ignore for extension fields).
An unchanged `RC_CHANNELS.chan6_raw` or `chan8_raw` is therefore expected with
the working command-based profile. `RC_CHANNELS` is FC telemetry, not an echo
of all GCS commands. Current FC parameter values require a separate read-only
bench inspection; this change performs no parameter writes and introduces no
RC5 arming behavior. The later user-authorized CH5 emergency disarm is handled from
physical USB button samples; it does not use FC RC5 telemetry or send RC5 override.

Camera Relay OFF/ON are built-in joystick actions and require no MAVLink actions
JSON. The custom handler runs before JSON lookup and selects the board's camera
path: Relay2/index 1 by default, or Herelink HDMI inputs 0/1 when enabled.
Existing JSON entries with the same labels do not create duplicate choices or
send an additional command. Other JSON actions keep their existing behavior.

## Recovery and persistence

The custom core plugin initializes settings before joystick polling starts.
For `OpenTX TX16SMK3 Joystick` (including numbered duplicates), exactly five
axes and seven buttons, it restores the documented actions only if all five
assignments 2–6 are `No Action`. A `HerelinkButtonProfileV1` marker in that
device's settings group makes this a one-time migration. Existing nonempty
profiles are preserved. Later intentional `No Action` edits remain cleared.
Calibration, axes, enabled vehicles, radio firmware and other applications'
settings are untouched. A new radio with no calibration still requires normal
calibration. The existing press/startup behavior is unchanged.

## Diagnostics and bench verification

Build with `custom\build-debug.cmd`. With the running application safely closed,
use `custom\build-debug.cmd run` to launch the rebuilt Debug executable with
diagnostics. This does not replace the installed release executable.
`build-herelink-debug/logs/latest-session.txt` points to the session containing
`commands.log`.

Look for these successive records:

1. `Restored Windows button profile`, if migration was needed, and settings path.
2. SDL discovery/active selection: hardware instance ID, display name and profile.
3. `Input device/button/action`: physical press and chosen action, including `No Action`.
4. `Button Action` / mode switch / camera request, then command queue and UDP writes.
5. `Outgoing RC_CHANNELS_OVERRIDE target/CH1-10` (at most once per second) for tilt;
   command/ACK records for mode, relay or HDMI selection instead of RC6/RC8 PWM.

With propellers removed, verify all three mode positions, both camera positions,
sticks and tilt. Restart GCS and repeat. Disconnect/reconnect the radio, then
replace it and repeat. With two radios attached, select the intended one and
verify its USB layout/calibration; the name suffix is not a permanent serial
identity. Confirm actual aircraft mode and picture as well as ACKs. Do not use
ARM/DISARM as part of this mapping check.

Automated coverage includes duplicate rescans, same-name replacement with
changed capabilities, active-manager replacement, settings reload/custom edits,
PWM endpoints/calibration, and decoding actual serialized override packets
from Vehicle through MockLink, including target IDs and release semantics.

## Validation on 2026-11-20

- Pinned Windows Debug build succeeded; output: `build-herelink-debug/Debug/HerelinkGCS.exe`.
- `ctest --output-on-failure -R '^(JoystickTest|JoystickManagerTest|HerelinkJoystickTest)$'`:
  all three suites passed. Log: `build-herelink-debug/rc-focused-final.log`.
- Final `ctest --output-on-failure -L Unit -LE 'Flaky|Network' --parallel 4`:
  191/198 passed. The six failures already listed in project context recurred
  (GeoTagControllerTest, BluetoothWorkerTest, QGCArchiveModelTest,
  QGCCompressionTest, PlatformTest, JsonResourceAuditTest; the last timed out).
  The unchanged HerelinkOsdTest also has outdated battery expectations
  (zero cells and total voltage versus the current fixed 12S/per-cell display).
  All joystick suites passed in this run. Log: `build-herelink-debug/rc-unit-final.log`.
- Changed-file pre-commit checks passed null safety, prohibited assertions/waits,
  categorized logging and secret checks. Full-file formatting/CMake lint still
  fail on existing style; the same source files from HEAD also fail clang-format.
  The clang-tidy hook expects the absent default `build/compile_commands.json`.
  New test files pass clang-format; new documentation passes markdownlint.
  Full hook output: `build-herelink-debug/rc-lint-final.log`.
- No live aircraft commands or parameter changes were made. The installed GCS
  was left running; the new executable must be launched for hardware validation.
