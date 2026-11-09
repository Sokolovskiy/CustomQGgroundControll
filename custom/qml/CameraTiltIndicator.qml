import QtQuick

Item {
    id: root
    property bool available: false
    property real degrees: NaN
    property string camera: "CAM —"
    property int requestedCamera: 0
    property color accent: "#ce9800"
    readonly property real unit: Math.min(width / 92, height / 116)
    readonly property bool valid: available && Number.isFinite(degrees)
    readonly property real angle: valid ? Math.max(0, Math.min(90, degrees)) : 0

    Rectangle {
        objectName: "herelinkOsdCameraCard"
        width: parent.width; height: 26 * root.unit
        radius: 7 * root.unit
        color: root.camera.startsWith("CAM1") ? "#000000"
            : root.camera.startsWith("CAM2") ? root.accent : "#99131e2c"
        Text {
            anchors.fill: parent; anchors.margins: 4 * root.unit
            text: root.camera.startsWith("CAM1") ? "CAM1 - MAIN"
                : root.camera.startsWith("CAM2") ? "CAM2 - STAB"
                : root.requestedCamera === 1 ? "CAM1 - MAIN"
                : root.requestedCamera === 2 ? "CAM2 - STAB" : "CAM —"
            color: root.camera.startsWith("CAM2") ? "#000000" : "white"
            font.pixelSize: 14 * root.unit; font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            fontSizeMode: Text.HorizontalFit; minimumPixelSize: 9
        }
    }
    Canvas {
        id: dial
        x: (parent.width - width) / 2; y: 31 * root.unit
        width: 80 * root.unit; height: 68 * root.unit
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(root.unit, root.unit)
            const cx = 26, cy = 16, radius = 41
            const radians = root.angle * Math.PI / 180
            ctx.strokeStyle = "#aab8c0"; ctx.lineWidth = 1
            ctx.beginPath(); ctx.arc(cx, cy, radius, 0, Math.PI / 2); ctx.stroke()
            if (root.valid) {
                ctx.fillStyle = "rgba(206, 152, 0, 0.4)"
                ctx.beginPath(); ctx.moveTo(cx, cy)
                ctx.arc(cx, cy, radius, 0, radians); ctx.closePath(); ctx.fill()
                ctx.strokeStyle = root.accent; ctx.lineWidth = 2
                ctx.beginPath(); ctx.moveTo(cx, cy)
                ctx.lineTo(cx + radius * Math.cos(radians), cy + radius * Math.sin(radians)); ctx.stroke()
            }
            ctx.strokeStyle = "#aab8c0"; ctx.lineWidth = 1
            for (let i = 0; i <= 6; ++i) {
                const a = i * Math.PI / 12
                ctx.beginPath()
                ctx.moveTo(cx + (radius - 4) * Math.cos(a), cy + (radius - 4) * Math.sin(a))
                ctx.lineTo(cx + radius * Math.cos(a), cy + radius * Math.sin(a)); ctx.stroke()
            }
            // Side-view camera pivots clockwise: horizontal is 0, down is 90.
            ctx.save(); ctx.translate(cx, cy); ctx.rotate(radians)
            ctx.fillStyle = "#c8cdd0"
            ctx.fillRect(-21, -10, 5, 22); ctx.fillRect(-15, -8, 19, 17)
            ctx.fillStyle = "#20292d"; ctx.beginPath(); ctx.arc(-1, 0, 4, 0, 2 * Math.PI); ctx.fill()
            ctx.restore()
            ctx.fillStyle = "white"; ctx.font = "11px sans-serif"
            ctx.fillText("0°", 67, 12); ctx.fillText("90°", 29, 67)
        }
        Connections {
            target: root
            function onAngleChanged() { dial.requestPaint() }
            function onValidChanged() { dial.requestPaint() }
            function onAccentChanged() { dial.requestPaint() }
            function onUnitChanged() { dial.requestPaint() }
        }
    }
    Text {
        anchors.bottom: parent.bottom
        width: parent.width
        text: root.valid ? Math.round(root.angle) + "°" : "—"
        color: root.valid ? root.accent : "#aab8c0"; font.pixelSize: 11 * root.unit
        horizontalAlignment: Text.AlignHCenter
    }
}
