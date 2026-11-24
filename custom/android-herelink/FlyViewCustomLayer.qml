import QtQuick
import QGroundControl
import "qrc:/Herelink"

Item {
    id: root

    property var parentToolInsets
    property var totalToolInsets: parentToolInsets
    property var mapControl

    NativeOsdModel {
        id: model
        vehicle: QGroundControl.multiVehicleManager.activeVehicle
        videoControl: QGroundControl.videoManager.videoStreamControl
        virtualJoystick: QGroundControl.settingsManager.appSettings.virtualJoystick.rawValue
    }

    HerelinkOsdView {
        anchors.fill: parent
        backend: model
        pipView: root.mapControl ? root.mapControl.pipView : null
        visible: !!root.mapControl && root.mapControl.pipState.state !== root.mapControl.pipState.fullState
        // Corner widgets do not reserve entire viewport edges for the OSD.
    }
}
