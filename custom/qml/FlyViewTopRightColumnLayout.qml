import QtQuick
import QtQuick.Layouts
import QGroundControl.Controls
import QGroundControl.FlyView

// Keep terrain status; omit only the native photo/video/settings control strip.
ColumnLayout {
    id: column
    spacing: ScreenTools.defaultFontPixelHeight / 2
    readonly property real osdScale: parent
        ? Math.max(0.4, Math.min(parent.width / 1148, parent.height / 646)) : 1

    TerrainProgress {
        Layout.fillWidth: true
        // Keep terrain status below the radio and ARM timer panels.
        Layout.topMargin: herelinkOsd.overlayVisible ? 82 * column.osdScale : 0
    }
}
