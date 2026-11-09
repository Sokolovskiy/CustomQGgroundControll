# TX16S initial mode synchronization: integration proposal

Status: source inspection complete; implementation pending approval of the
upstream integration below. No flight commands were executed.

## Findings

- Joystick::_getButton is private. There is no public current-state getter.
- Joystick::_handleButtons emits rawButtonPressedChanged only for transitions
  while PollingForConfiguration is set. Starting configuration polling would
  change the normal control path and is not an acceptable observation method.
- Joystick::run calls _handleButtons after each successful _update. This is
  the correct place to obtain a fresh, coherent snapshot without another SDL
  reader or a second polling thread.
- Initial button states are ButtonEventNone. A held button can become a down
  transition and execute its normal mapped action on the first sample.
  Consequently, a custom observer alone cannot guarantee no initial Land.
- Vehicle::setFlightMode uses the firmware plugin and existing command path.
  Mode confirmation must come from subsequent vehicle telemetry. A timeout
  means unconfirmed, not necessarily an explicit flight-controller rejection.

## Smallest proposed upstream integration

Change only src/Joystick/Joystick.h and src/Joystick/Joystick.cc:

1. Add a read-only, timestamped button snapshot notification from the existing
   successful polling path, with polling-session generation and target vehicle
   identity. Invalidate it on polling stop, update failure, or device removal.
   Include whether vehicle or configuration polling produced the snapshot.
2. Add an optional initialization action gate, supplied by the custom build.
   It must apply before an action is queued, and be checked again before
   dispatch to reject stale queued work. With no gate, upstream behavior stays
   unchanged. Do not change axis output, RC output, calibration, mappings,
   button repeat behavior, or transport.
3. When synchronization is enabled, the custom gate consumes initial held
   CH6 mode actions until a valid initialization decision is completed. This
   prevents the existing manual Land mapping from firing at initialization.
   Real subsequent switch transitions retain all three manual mappings.
   Releasing a gate must never replay a held button as a new press.

An observer-only signal is insufficient for requirements 4, 9, 10 and test 12.
No direct SDL access, raw MAVLink construction, or vehicle-source change is
proposed.

## Custom implementation

- Add a minimal QGCCorePlugin subclass that inherits all standard behavior
  and starts a custom mode-sync controller from init(). Keep both firmware
  factories enabled and preserve standard Fly View and settings pages.
- Add a persistent Fact-backed Herelink setting, disabled by default, exposed
  on a small custom settings page. Enabling while already connected must not
  immediately change mode; take effect on the next initialization cycle.
- Restrict eligibility to the active ArduPilot Copter and verified TX16S
  device/mapping. Validate the numbering against QGC's button labels and stored
  actions; SDL indices are zero-based. Do not guess from a different joystick
  or fall back to another device or flight mode.
- Identify cycles by vehicle connection generation and joystick polling
  generation. Switching active vehicles must not re-arm an already consumed
  cycle. Inactive vehicles receive no commands. USB reconnection and actual
  telemetry reconnection invalidate input and permit a new eligible cycle.
- Require initial connection complete, fresh heartbeat/state, communication
  healthy, joystick enabled and calibrated, and normal vehicle polling.
  Require repeated fresh samples of exactly one CH6 state for 500 ms, with
  no sample gap over 250 ms. Recheck identity, readiness, armed state and
  snapshot age immediately before the request on the application thread.
- Observing ARMED consumes the cycle without synchronization. Later disarming
  does not trigger a deferred request. Manual mode selection or a physical
  switch transition after initialization observation begins cancels automatic
  initialization for that cycle, preventing a delayed operator override.
- Stable initial Land: consume cycle, warn, send nothing. Stable Altitude Hold
  or Position Hold: resolve through the existing firmware mode list, skip if
  already current, otherwise consume cycle before Vehicle::setFlightMode.
- A bounded initialization window expires with a warning on ambiguous or
  unavailable input. Never retry indefinitely while waiting for a valid state.
- Observe command result where available and flightModeChanged. Report explicit
  rejection distinctly from a bounded confirmation timeout. Cancel confirmation
  on manual intervention, arming, disconnect or identity change. Never resend
  or select a fallback mode.
- Log cycle identity, readiness/cancellation reasons, freshness, stable state,
  request, telemetry confirmation, and failure. Use existing application
  warning UI for blocked Land, unavailable mode and unconfirmed/rejected mode.

The armed check guarantees that the GCS does not issue an automatic request
while its latest confirmed vehicle state is armed. It cannot make a MAVLink
mode request atomic with physical arming at the flight controller; telemetry
freshness and cancellation checks reduce that race without changing FC logic.

## Verification plan

Use a deterministic state-machine test suite with synthetic timestamps,
snapshots and vehicle states for all twelve requested scenarios. Add dispatch
integration tests with MockJoystick/MockLink proving that initial held Land
and duplicate polling startup cannot reach Vehicle::setFlightMode, and that
later real manual transitions retain their mappings. Exercise stale queued
events, active-vehicle switching, configuration polling, manual override,
arming during debounce and while awaiting confirmation, and disabled feature.
No physical vehicle commands in automated tests.

Build using custom/build-debug.cmd, run the full Unit label and the new tests,
and compare with the recorded six upstream failures (191/197 baseline).
Preserve the existing upstream build directory and its executable.

Hardware validation after implementation: propellers removed, vehicle
disarmed, manual settings unchanged. Verify feature disabled first; then enable
and reconnect separately in each CH6 position. Confirm one request for Altitude
Hold/Position Hold, zero for Land, no duplicate requests on polling startup,
and normal later manual switch actions. Repeat telemetry and USB reconnects,
ambiguous switch states and manual mode changes during initialization. Verify
armed behavior with mocks only before any separately authorized hardware test.
