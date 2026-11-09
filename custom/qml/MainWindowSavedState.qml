import QtQuick
import QtQuick.Window
import QtCore

import QGroundControl
import QGroundControl.Controls

// Custom replacement for QGC's single main-window state owner.
Item {
    id: root
    property Window window

    readonly property bool _desktop: !ScreenTools.isMobile && !ScreenTools.isFakeMobile
    readonly property bool _enabled: _desktop && QGroundControl.corePlugin.options.enableSaveMainWindowPosition
    property bool _initialized: false
    property bool _changingMode: false
    property rect _normalGeometry: Qt.rect(0, 0, 0, 0)
    property int _returnVisibility: Window.Windowed
    property int _lastStableVisibility: Window.Windowed

    Settings {
        id: s
        category: "MainWindowState"
        // Keep QGC's existing keys; fullscreen geometry must never replace these.
        property int x
        property int y
        property int width
        property int height
        property int visibility
        property int restoreVisibility: Window.Windowed
    }

    function _setDefaultDesktopWindowSize() {
        window.width = Math.min(250 * Screen.pixelDensity, Screen.width)
        window.height = Math.min(150 * Screen.pixelDensity, Screen.height)
        window.x = Screen.virtualX + (Screen.width - window.width) / 2
        window.y = Screen.virtualY + (Screen.height - window.height) / 2
    }

    function _captureNormalGeometry() {
        if (window && window.visibility === Window.Windowed && window.width > 0 && window.height > 0) {
            _normalGeometry = Qt.rect(window.x, window.y, window.width, window.height)
        }
    }

    function _applyNormalGeometry() {
        if (!window || _normalGeometry.width <= 0 || _normalGeometry.height <= 0)
            return
        window.x = _normalGeometry.x
        window.y = _normalGeometry.y
        window.width = _normalGeometry.width
        window.height = _normalGeometry.height
    }

    function _storeSettings(mode, flush) {
        if (!_enabled || !_initialized)
            return
        if (_normalGeometry.width > 0 && _normalGeometry.height > 0) {
            s.x = Math.round(_normalGeometry.x)
            s.y = Math.round(_normalGeometry.y)
            s.width = Math.round(_normalGeometry.width)
            s.height = Math.round(_normalGeometry.height)
        }
        s.restoreVisibility = _returnVisibility
        s.visibility = mode
        if (flush)
            s.sync()
    }

    function saveSettings(flush = false) {
        if (!_enabled || !_initialized || _changingMode || !window)
            return
        const mode = window.visibility
        if (mode !== Window.Windowed && mode !== Window.Maximized && mode !== Window.FullScreen)
            return
        if (mode === Window.Windowed)
            _captureNormalGeometry()
        _storeSettings(mode, flush)
    }

    function _geometryChanged() {
        if (!_initialized || _changingMode || !window)
            return
        // Capture live normal bounds before a fast maximize/fullscreen sequence;
        // the debounced disk write alone can miss the last windowed rectangle.
        _captureNormalGeometry()
        if (_enabled)
            saveSettingsTimer.restart()
    }

    function toggleFullScreen() {
        if (!_desktop || !_initialized || _changingMode || !window)
            return
        saveSettingsTimer.stop()
        const exiting = window.visibility === Window.FullScreen
            || (window.visibility === Window.Minimized && _lastStableVisibility === Window.FullScreen)
        if (!exiting) {
            _captureNormalGeometry()
            _returnVisibility = window.visibility === Window.Maximized
                || (window.visibility === Window.Minimized && _lastStableVisibility === Window.Maximized)
                ? Window.Maximized : Window.Windowed
            saveSettings(true)
        }
        _changingMode = true
        if (exiting) {
            if (_returnVisibility === Window.Maximized) {
                window.showMaximized()
            } else {
                window.showNormal()
                _applyNormalGeometry()
            }
        } else {
            window.showFullScreen()
        }
        // Persist the requested mode using cached normal bounds even if a close
        // arrives before the deferred native-transition completion below.
        _storeSettings(exiting ? _returnVisibility : Window.FullScreen, true)
        // Let Qt finish the native state transition before saving normal bounds.
        Qt.callLater(function() {
            if (!root.window)
                return
            if (exiting && root._returnVisibility === Window.Windowed
                && root.window.visibility === Window.Windowed) {
                root._applyNormalGeometry()
            }
            root._lastStableVisibility = root.window.visibility
            root._changingMode = false
            root.saveSettings(true)
        })
    }

    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        autoRepeat: false
        enabled: root._desktop && root._initialized && root.window !== null
        onActivated: root.toggleFullScreen()
    }

    Connections {
        target: root.window
        function onXChanged() { root._geometryChanged() }
        function onYChanged() { root._geometryChanged() }
        function onWidthChanged() { root._geometryChanged() }
        function onHeightChanged() { root._geometryChanged() }
        function onVisibilityChanged() {
            if (!root._initialized || root._changingMode || !root.window)
                return
            const mode = root.window.visibility
            if (mode === Window.FullScreen) {
                // Also honor native fullscreen transitions outside this shortcut.
                root._returnVisibility = root._lastStableVisibility === Window.Maximized
                    ? Window.Maximized : Window.Windowed
            }
            if (mode === Window.Windowed || mode === Window.Maximized || mode === Window.FullScreen) {
                root._lastStableVisibility = mode
                root.saveSettings(true)
            }
        }
        function onClosing(close) {
            // Only persist state; MainWindow retains all close/safety checks.
            root.saveSettings(true)
        }
    }

    Timer {
        id: saveSettingsTimer
        interval: 500
        repeat: false
        onTriggered: root.saveSettings()
    }

    Component.onCompleted: {
        if (!window)
            return
        _changingMode = true
        if (ScreenTools.isFakeMobile) {
            window.width = ScreenTools.screenWidth
            window.height = ScreenTools.screenHeight
        } else if (ScreenTools.isMobile) {
            window.showFullScreen()
        } else {
            if (_enabled) {
                window.minimumWidth = Math.min(ScreenTools.defaultFontPixelWidth * 100, Screen.width)
                window.minimumHeight = Math.min(ScreenTools.defaultFontPixelWidth * 50, Screen.height)
            }
            if (_enabled && s.width > 0 && s.height > 0) {
                _normalGeometry = Qt.rect(s.x, s.y, s.width, s.height)
                _applyNormalGeometry()
            } else {
                _setDefaultDesktopWindowSize()
                _normalGeometry = Qt.rect(window.x, window.y, window.width, window.height)
            }
            _returnVisibility = _enabled && s.restoreVisibility === Window.Maximized
                ? Window.Maximized : Window.Windowed
            if (_enabled && s.visibility === Window.FullScreen)
                window.showFullScreen()
            else if (_enabled && s.visibility === Window.Maximized)
                window.showMaximized()
            else
                window.showNormal()
        }
        _lastStableVisibility = window.visibility
        _initialized = true
        _changingMode = false
    }
}
