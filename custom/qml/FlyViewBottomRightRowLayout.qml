import QtQuick
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

// Keep native telemetry/instruments on the map and when OSD is off. On main
// video, avoid duplicate instruments without altering any warning controls.
RowLayout {
    readonly property bool customOsdActive: herelinkOsd.overlayVisible && _mapControl
        && _mapControl.pipState.state !== _mapControl.pipState.fullState

    TelemetryValuesBar {
        visible: !parent.customOsdActive
        Layout.alignment: Qt.AlignBottom
        extraWidth: instrumentPanel.extraValuesWidth
        settingsGroup: factValueGrid.telemetryBarSettingsGroup
        specificVehicleForCard: null
    }
    FlyViewInstrumentPanel {
        id: instrumentPanel
        Layout.alignment: Qt.AlignBottom
        visible: !parent.customOsdActive && QGroundControl.corePlugin.options.flyView.showInstrumentPanel && _showSingleVehicleUI
    }
}
