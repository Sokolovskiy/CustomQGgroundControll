import QtQuick

Item {
    id: root
    property var parentToolInsets
    property var totalToolInsets: parentToolInsets
    property var mapControl
    readonly property bool videoMain: mapControl && mapControl.pipState.state !== mapControl.pipState.fullState

    HerelinkOsdView {
        anchors.fill: parent
        visible: root.videoMain
        pipView: root.mapControl ? root.mapControl.pipView : null
        // Corner-widget insets are not full-edge margins. PiP retains its size
        // when collapsed, and the hidden multi-vehicle panel still reports an
        // inset. Keep OSD anchored to the viewport, as in the reference layout.
        // Fly View already places this layer below the main toolbar.
    }
}
