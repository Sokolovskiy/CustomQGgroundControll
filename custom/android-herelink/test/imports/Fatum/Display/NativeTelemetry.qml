import QtQuick

QtObject {
    property var channels: [1500, 1500, 1000, 1500]
    property bool channelsAvailable: true
    property bool attitudeAvailable: true
    property string radioRssi: "-65"
    property string radioSnr: "30"
    property real yawReference: NaN
    property var motion: ({ available: false, visible: false,
                           rotation: Qt.quaternion(1, 0, 0, 0), speed: 0, axes: "—" })

    function axis(mapping: string): real {
        return mapping === "RCMAP_THROTTLE" ? -1 : 0
    }
}
