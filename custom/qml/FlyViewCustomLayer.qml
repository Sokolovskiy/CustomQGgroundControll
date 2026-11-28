import QtQuick
import QtMultimedia
import QGroundControl

Item {
    id: root
    property var parentToolInsets
    property var totalToolInsets: parentToolInsets
    property var mapControl
    readonly property bool videoMain: mapControl && mapControl.pipState.state !== mapControl.pipState.fullState

    readonly property bool scorpionPipEnabled: root.videoMain
        && herelinkOsd.cameraProfileAvailable && herelinkOsd.herelinkHdmi
        && !!QGroundControl.multiVehicleManager.activeVehicle
        && !QGroundControl.multiVehicleManager.activeVehicle.communicationLost

    onScorpionPipEnabledChanged: QGroundControl.videoManager.setScorpionPipEnabled(scorpionPipEnabled)
    Component.onCompleted: QGroundControl.videoManager.setScorpionPipEnabled(scorpionPipEnabled)
    Component.onDestruction: QGroundControl.videoManager.setScorpionPipEnabled(false)

    HerelinkOsdView {
        anchors.fill: parent
        visible: root.videoMain
        pipView: root.mapControl ? root.mapControl.pipView : null
        // Corner-widget insets are not full-edge margins. PiP retains its size
        // when collapsed, and the hidden multi-vehicle panel still reports an
        // inset. Keep OSD anchored to the viewport, as in the reference layout.
        // Fly View already places this layer below the main toolbar.
    }

    Rectangle {
        id: scorpionPip
        visible: root.scorpionPipEnabled && QGroundControl.videoManager.scorpionPipAvailable
        width: Math.min(root.width * 0.28, 380)
        height: width * 9 / 16
        x: Math.max(0, root.width - width - 12)
        y: Math.min(68, Math.max(0, root.height - height))
        color: "black"
        border.color: "#ce9800"
        border.width: 1
        radius: 4
        clip: true
        z: 10

        VideoOutput {
            objectName: "scorpionPipVideo"
            anchors.fill: parent
            anchors.margins: 2
            fillMode: VideoOutput.PreserveAspectFit
        }
    }
}
