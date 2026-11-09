import QtQuick

Item {
    id: root
    property string label
    property bool available: false
    property real horizontal: 0
    property real vertical: 0
    Text {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: "white"
        font.pixelSize: parent.height * 0.19
    }
    Rectangle {
        id: square
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.9
        height: width
        color: "transparent"
        border.color: root.available ? "#aab8c0" : "#58636c"
        radius: width * 0.18
        Rectangle { width: parent.width; height: 1; anchors.centerIn: parent; color: "#72828a" }
        Rectangle { height: parent.height; width: 1; anchors.centerIn: parent; color: "#72828a" }
        Rectangle {
            visible: root.available
            width: parent.width * 0.19
            height: width
            radius: width / 2
            color: "#ce9800"
            x: (parent.width - width) / 2 + Math.max(-1, Math.min(1, root.horizontal)) * (parent.width - width) / 2
            y: (parent.height - height) / 2 - Math.max(-1, Math.min(1, root.vertical)) * (parent.height - height) / 2
        }
        Text { anchors.centerIn: parent; visible: !root.available; text: "—"; color: "#aab8c0"; font.pixelSize: square.width * 0.3 }
    }
}
