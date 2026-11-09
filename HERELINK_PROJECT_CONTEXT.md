# Herelink GCS project context

Snapshot from 2026-11-07. This document records the project's technical decisions, current implementation, and open bench checks. See [AGENTS.md](AGENTS.md) for the investigation scope.

## Build and run

- Herelink GCS is the Windows x64 custom build of QGroundControl v5.1.5. Keep MSVC v143 **14.44.35207**, Qt **6.11.1** `msvc2022_64`, and GStreamer **1.28.4**. Keep GStreamer enabled for RTSP/H.264. Ask before installing system-wide software or changing dependency versions.
- The preserved upstream executable is `build-upstream-v5.1.5-debug-clean/Debug/QGroundControl.exe`. Earlier dependency failures in the [initial Windows audit](custom-example/HERELINK_WINDOWS_BUILD.md) are historical.
- Custom builds use `build-herelink-debug` and the executable is `build-herelink-debug/Debug/HerelinkGCS.exe`. Run from the repository root in PowerShell:

  ```powershell
  .\custom\build-debug.cmd
  .\custom\build-debug.cmd run
  ```

- The launch command writes `commands.log`, `stdout.log`, and `session.json` under a new `build-herelink-debug/logs` session. Read `latest-session.txt` there to locate it. The user has authorized launching the build. If a connected application is locking the EXE, let the user close it when safe.
- Keep source comments in English. Prefer the `custom/` overlay, resource overrides, and existing hooks. The upstream baseline was successfully built and launched before customization. The custom app uses its own display name and settings. `custom-example/` was a structural reference. It is PX4-oriented, so preserve the standard firmware factories and full ArduPilot support.
- Preserve MAVLink/UDP, SDL joystick calibration and input, video decoding, preflight checks, warnings, and failsafes. FC parameter changes require specific approval for the named board and parameters. Honor the user's instruction to skip automated tests for the recent changes. Test commands and the original comparison are in the [custom build guide](custom/README.md#windows-commands).

The original baseline passed 191 of 197 Unit tests. The six known failures are `GeoTagControllerTest`, `BluetoothWorkerTest`, `QGCArchiveModelTest`, `QGCCompressionTest`, `JsonResourceAuditTest`, and `PlatformTest`. The initial custom build reproduced those six. This is historical evidence, so report any later verification separately.

## Hardware and controls

The Windows PC normally uses two Herelink units in AirUnit-to-AirUnit mode, an ArduPilot flight controller, and a RadioMaster TX16S over USB/SDL. The handheld Herelink controller is a separate setup and can send SBUS to the FC. The official application verified telemetry, video and joystick integration. The PC setup observed in diagnostic logs uses UDP port `14552` and GroundUnit peer `192.168.144.11`. Keep those values scoped to the observed setup.

- ARM and DISARM are manual in the Fly View readiness panel. CH5 is unassigned for PC control. The earlier CH5 interlock request was superseded. Keep standard pre-arm and emergency behavior.
- The Fly View mode menu offers Altitude Hold, Position Hold, Land and RTL when available from the firmware. The main toolstrip hides Takeoff and Return/RTL and retains Land where eligible. These are UI restrictions. Standard vehicle capabilities still exist.
- The existing PC TX16S setup showed joystick actions 2/3/4 for Altitude Hold/Position Hold/Land and 5/6 for camera off/on. Read the live joystick configuration for the actual mapping. On the handheld Herelink, the user observed RC6 at 1000 for Position Hold and 2000 for Altitude Hold. Keep the handheld channel semantics distinct from PC joystick action labels.
- The user asked to explore an RC/PWM profile for CH5–CH10, then rolled the migration back after regressions. Do not treat the proposed RC11 startup override or CH7/CH9 fallback as active behavior. CH6 startup synchronization with automatic LAND blocked was requested, but the [proposal](custom/MODE_SYNC_PROPOSAL.md) and the [custom build guide](custom/README.md) describe it as unimplemented. Verify the targeted code before relying on that protection.
- The analog camera control was described as transmitter CH11. The current Windows profile maps USB axis 4 through Additional Axis 6 to outgoing MAVLink RC10. The OSD reports commanded tilt. The user explicitly specified 1000 PWM → 0° in chat, while the current code and [tilt note](custom/CAMERA_TILT.md) map 1000 → 90° and 2000 → 0°. Resolve this discrepancy against the physical endpoint before changing the control or relying on the displayed angle.

## Aircraft-specific behavior

Both aircraft exports identified ArduCopter 4.6.3 and MAVLink vehicle/component `1/1`. Route camera profiles by hardware identity. The camera-profile key uses the legacy UID where available and falls back to `AUTOPILOT_VERSION.uid2` on ArduPilot.

| Aircraft | Camera path | Relevant evidence |
| --- | --- | --- |
| Verba | FC Relay2 | MAVLink relay index `1`, `RELAY2_FUNCTION=1`, `RELAY2_PIN=82`, `RC8_OPTION=34`. OFF selects CAM1 MAIN and ON selects CAM2 STAB per the documented wiring. Verba accepted `MAV_CMD_DO_SET_RELAY`. Preserve this default path. |
| Scorpion | Herelink AirUnit HDMI 1 / HDMI 2 | Both cameras feed the AirUnit directly. Relay2 is disabled and the FC rejected relay commands. In DISARMED, right-click **OSD** and enable **Herelink HDMI** once for this board. The same TX16S positions select input indices `0/1` via `MAV_CMD_VIDEO_START_STREAMING` to the observed Herelink camera component `42/100` on the active link. The RTSP address stays the same. |

- Scorpion initially reported `PreArm: RC not found` while GCS was sending as system `255` and Scorpion had `SYSID_MYGCS=1`. The user approved only changing that parameter to `255` on Scorpion and subsequently reported that ARM worked. Keep that approval scoped to that action. Verba already used `255`.
- The earlier Scorpion telemetry log `2026-11-07 17-45-59.tlog` contained 31 camera ACKs for 38 start-streaming commands. Usual delayed ACKs were about 0.55–0.71 seconds. No active-stream information appeared in that capture. An accepted ACK establishes that the AirUnit acknowledged the command. Physical picture selection is a separate bench observation.
- The latest build serializes HDMI commands, retains the newest switch position, and makes one repeat after about 0.9 seconds when the current command has no response and has not been superseded. It shows the reported or acknowledged camera. The requested camera is marked with `?` while unconfirmed. Startup remains unknown until there is evidence. The latest Debug build and launch succeeded. The updated switch reliability and actual HDMI picture changes still require a Scorpion bench check.

## OSD and UI decisions

- The video-first OSD uses translucent edge-anchored panels, accent `#ce9800`, and the FATUM image while waiting for video. Keep the video aspect ratio. F11 uses Qt native fullscreen and persists the prior normal/maximized state and geometry. See [fullscreen notes](custom/FULLSCREEN.md).
- The active mode has a separate panel at the top left. Altitude Hold uses a black background, Position Hold the accent, and Land green. The mode panel border follows the configured mode color. The arming badge is above the 3D model: DISARMED is black on white, ARMED is white on red, and NO VEHICLE uses the neutral OSD background. The right-side camera recording/settings panel was removed from Fly View.
- The OSD shows 12S average per-cell voltage and current. Apply red below 3.4 V/cell and yellow from 3.4 to 3.6 V/cell to the voltage digits. Display throttle percentage from telemetry. Keep the callsign centered at the top when available, read `/callsign.txt` from the FC SD card through MAVLink FTP, and hide its panel when the text is unavailable.
- Bottom left contains the sticks, timer, camera badge, and commanded tilt dial. CAM1 MAIN uses black and CAM2 STAB uses the accent. Bottom right contains the 3D attitude model and compact telemetry. Tilt arrows indicate the lowered side near the applicable motor, with deviations up to 1° ignored. The cyan motion arrow uses measured motion telemetry. Keep warnings and readiness messaging accessible. See [OSD history](custom/OSD.md), [3D attitude](custom/ATTITUDE_3D.md), and [tilt notes](custom/CAMERA_TILT.md).

## Command diagnostics and open checks

- Some flight-mode, relay, and normal ARM/DISARM actions previously failed intermittently in the custom GCS. The user reported stable operation in Mission Planner and in the official QGC/Herelink controller setup. Those are separate setups. The transport loss point was not conclusively isolated.
- See [command diagnosis](custom/COMMAND_DIAGNOSIS.md) for UDP diagnostics and bounded command repeats. The custom ARM/DISARM mitigation was specifically approved for one normal retry at 100 ms with a 250 ms cutoff and cancellation on state/link changes. Never substitute force-arm, repeat an explicit refusal, or change failsafe behavior to resolve a command issue.
- For Scorpion, verify HDMI 1 → HDMI 2 → HDMI 1 while DISARMED. Compare the physical picture, OSD, and camera-component ACKs. Verify Verba still uses Relay2 when its HDMI profile is off. Handle any remaining camera servo issue separately from HDMI selection and compare the actual control endpoint before making changes.
