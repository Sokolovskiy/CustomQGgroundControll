# Native main-window fullscreen

Initially implemented as source changes only, then built and launched after
the user's explicit approval. custom/build-debug.cmd completed successfully
(exit code 0); log: build-herelink-native-fullscreen.log. The updated executable
was launched in diagnostic session logs/2026-11-05_18-44-34-560_c2c54ab2.
No QML loading errors were found. Settings subsequently recorded FullScreen
visibility (5) and Maximized restoreVisibility (4), confirming the custom state
helper is active. UI input verification was interrupted by detected user input;
full mode/geometry and restart verification remains incomplete. No automated
tests or physical flight/control commands were issued.

## Existing mechanism and integration

MainWindow.qml is a native Qt Quick Controls ApplicationWindow. It instantiates
one MainWindowSavedState helper, which already owns desktop window persistence
in the MainWindowState settings category. The module's generated qmldir maps
that helper to MainWindow/MainWindowSavedState.qml under
qrc:/qml/QGroundControl/.

HerelinkResourceInterceptor redirects that helper to
qrc:/Herelink/MainWindowSavedState.qml. The original MainWindow.qml and Fly View
are not replaced. There is one instantiated state owner and one F11 shortcut;
no parallel C++ window controller, Win32 window manipulation or key filter is
introduced. Existing MainWindow close checks and vehicle logic remain intact.

## Toggle and restoration

F11 is a Qt Quick Shortcut with Qt.ApplicationShortcut context, so it does not
depend on which video/QML child has keyboard focus. Auto-repeat is disabled.
No other key sequences or Escape handling are added. The shortcut is active
only for desktop builds and uses the main ApplicationWindow's native
showFullScreen(), showNormal() and showMaximized() methods. Qt's Windows
platform implementation owns title-bar, border and taskbar behavior. No
FramelessWindowHint, fake maximized fullscreen or manual taskbar hiding is used.

Before entry the helper captures windowed bounds and remembers whether the
previous mode was Windowed or Maximized. Fullscreen/maximized bounds do not
replace the saved normal rectangle. Exit returns to Maximized when appropriate;
otherwise showNormal() is followed by restoration of the captured x/y/width/
height. A guarded Qt.callLater completes the native transition before saving
again. Shortcuts arriving during that transition are ignored rather than
running overlapping geometry changes.

The existing MainWindowState x/y/width/height/visibility keys are retained.
One additional key, restoreVisibility, records the mode to use when exiting
fullscreen, including after an application restart. Stored FullScreen visibility
restores fullscreen at startup. Toggle completion and window-closing events
flush settings; closing only saves state and never accepts or bypasses QGC's
close confirmation/safety checks. Hidden/minimized transient states do not
overwrite the persisted desktop preference. Mobile/fake-mobile initialization
and the enableSaveMainWindowPosition option remain respected.

## Viewport, OSD and video

This operates on the native main window, not VideoManager.fullScreen. The
latter is QGC's separate double-click video-only presentation mode and hides
FlyViewWidgetLayer/FlyViewCustomLayer. F11 does not set that flag or change its
existing shortcut/behavior, so it does not hide the custom OSD or safety toolbar.
Existing user-selected OSD/video-only visibility preferences remain effective.

The native window's client area expands to the display. Existing anchors.fill
chains resize Fly View and the video viewport. The custom layer already fills
the widget viewport and HerelinkOsdView derives its scale/positions from width
and height, so top/bottom/edge cards and the control deck reflow automatically.
The native QGC toolbar retains its normal place inside that application area.

Video rendering and user video settings are not changed. Inspection found the
standard videoFit default is Fit Height (1). FlightDisplayViewVideo computes
video dimensions using its aspect ratio; FlightDisplayViewVideoOutput uses
VideoOutput.PreserveAspectFit except for the user-selected Fill mode, which
uses PreserveAspectCrop. These preserve proportions rather than stretching.
Fullscreen does not force a new aspect ratio, reset an existing fit choice,
restart decoding or alter the GStreamer pipeline. Letterboxing/cropping follows
the existing aspect-preserving fit policy and source/stream metadata.

## Files

Created:

- custom/qml/MainWindowSavedState.qml
- custom/FULLSCREEN.md

Modified:

- custom/src/HerelinkCorePlugin.cc
- custom/custom.qrc
- custom/README.md

No upstream source, control logic, MAVLink, telemetry, joystick, camera relay,
video-decoder or FC parameter files are changed for this feature. Existing
unrelated working-tree modifications are retained.

## Deferred manual verification

Compilation succeeded, but the complete runtime checklist remains outstanding:

1. Resize/move a normal window, immediately press F11, then F11 again: compare
   the restored normal rectangle, including a fast resize/maximize sequence.
2. Enter from Maximized, leave with F11, and confirm Maximized returns. Restore
   down afterward and confirm the original normal rectangle is still available.
3. Restart while fullscreen was the saved mode; F11 must restore the saved
   normal/maximized mode and normal geometry rather than fullscreen bounds.
4. Press F11 with video, editable fields, Fly/Plan/Settings and ordinary popup
   focus. Verify a held F11 does not repeatedly toggle and existing shortcuts
   and video-only double-click behavior remain intact.
5. Check title bar/borders/taskbar, OSD anchors, native toolbar access, video
   proportions, DPI changes and multiple-monitor transitions on Windows 11.
   Monitor removal and OS-specific geometry clamping need real desktop checks.
6. Confirm normal close prompts remain visible and persisted settings survive
   closing immediately after a toggle. Do not issue flight commands to check
   window-management behavior.

On upstream upgrades compare this small helper with the upstream
MainWindowSavedState.qml and recheck its resource path in the module qmldir.
