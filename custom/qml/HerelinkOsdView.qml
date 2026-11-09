import QtQuick
import QtQuick.Controls
import QGroundControl
import QGroundControl.Controls

Item {
    id: root
    objectName: "herelinkOsdView"
    property var backend: herelinkOsd
    property var pipView: null
    readonly property var d: backend.data
    property real leftReserve: 0
    property real rightReserve: 0
    property real bottomReserve: 0
    readonly property real availableWidth: Math.max(0, width - leftReserve - rightReserve)
    // Dimensions follow the user's 1148 x 646 reference composition.
    readonly property real unit: Math.max(0.4, Math.min(availableWidth / 1148, (height - bottomReserve) / 646))
    readonly property real gap: 8.5 * unit
    readonly property color accent: "#ce9800"
    readonly property color caution: "#ffbd4b"
    readonly property color danger: "#ff5b57"
    readonly property var vectors: backend.attitudeVectors

    function attitudeOverlapsPip() {
        const content = pipView ? pipView._pipContentItem : null
        if (!content || !content.visible)
            return false
        // The loader now lives inside the combined control card.
        const origin = content.mapToItem(attitudeLoader, 0, 0)
        return origin.x < attitudeLoader.width && origin.x + content.width > 0
            && origin.y < attitudeLoader.height && origin.y + content.height > 0
    }

    component Readout: Text {
        color: "white"
        font.pixelSize: 18 * root.unit
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    component ReadoutIcon: QGCColoredImage {
        width: 16 * root.unit
        height: width
        anchors.verticalCenter: parent.verticalCenter
    }

    Component {
        id: attitudeComponent
        DroneAttitude3D {
            attitude: root.backend.attitudeRotation
            rollDegrees: root.backend.attitudeRoll
            pitchDegrees: root.backend.attitudePitch
            vectors: root.vectors
            accentColor: root.accent
            embedded: true
            readoutHeight: 0
        }
    }

    Item {
        id: graphics
        objectName: "herelinkOsdGraphics"
        anchors.fill: parent
        visible: root.backend.overlayVisible

        OsdPanel {
            id: flightModeCard
            objectName: "herelinkOsdStateCard"
            x: root.leftReserve + root.gap
            y: 10 * root.unit
            width: 170 * root.unit; height: 38 * root.unit
            readonly property bool altitudeHold: root.d.connected
                && root.d.mode === qsTranslate("ArduCopterFirmwarePlugin", "Altitude Hold")
            readonly property bool positionHold: root.d.connected
                && root.d.mode === qsTranslate("ArduCopterFirmwarePlugin", "Position Hold")
            readonly property bool land: root.d.connected
                && root.d.mode === qsTranslate("ArduCopterFirmwarePlugin", "Land")
            readonly property bool lightBackground: positionHold || land
            color: altitudeHold ? "#000000" : positionHold ? root.accent : land ? "#69ff37" : "#99131e2c"
            border.color: color
            Readout {
                objectName: "herelinkOsdFlightModeCard"
                anchors.fill: parent; anchors.margins: 10 * root.unit
                text: root.d.mode
                color: flightModeCard.lightBackground ? "black" : root.accent
                font.pixelSize: 19 * root.unit
                horizontalAlignment: Text.AlignHCenter
                fontSizeMode: Text.HorizontalFit; minimumPixelSize: 10
            }
        }

        OsdPanel {
            id: callsignCard
            visible: root.backend.callsign.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            y: 10 * root.unit
            width: 160 * root.unit; height: 36 * root.unit
            Readout {
                anchors.fill: parent; anchors.margins: 8 * root.unit
                text: root.backend.callsign
                font.pixelSize: 21 * root.unit
                horizontalAlignment: Text.AlignHCenter
                fontSizeMode: Text.HorizontalFit; minimumPixelSize: 10
            }
        }

        OsdPanel {
            id: radioCard
            x: root.width - root.rightReserve - width - root.gap
            y: 10 * root.unit
            width: 244 * root.unit; height: 34 * root.unit

            function signalColor(reading: string, excellent: int, good: int, fair: int, weak: int): color {
                const value = Number(reading)
                if (!Number.isFinite(value))
                    return "white"
                if (value >= excellent)
                    return "#22c55e"
                if (value >= good)
                    return "#84cc16"
                if (value >= fair)
                    return "#facc15"
                if (value >= weak)
                    return "#fb923c"
                return root.danger
            }

            Row {
                id: radioRow
                anchors.fill: parent; anchors.leftMargin: 9 * root.unit; anchors.rightMargin: 9 * root.unit
                spacing: 7 * root.unit
                ReadoutIcon { source: "qrc:/qmlimages/Signal100.svg" }
                Readout {
                    width: 87 * root.unit; height: parent.height
                    text: qsTr("RSSI %1 dBm").arg(root.d.radioRssi)
                    color: radioCard.signalColor(root.d.radioRssi, -60, -70, -75, -80)
                    font.pixelSize: 14 * root.unit
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 8
                }
                Rectangle { width: 1; height: 18 * root.unit; anchors.verticalCenter: parent.verticalCenter; color: "#72828a" }
                ReadoutIcon { source: "qrc:/Herelink/antenna.svg" }
                Readout {
                    width: radioRow.width - 119 * root.unit - 4 * radioRow.spacing - 1
                    height: parent.height
                    text: qsTr("SNR %1 dB").arg(root.d.radioSnr)
                    color: radioCard.signalColor(root.d.radioSnr, 30, 25, 15, 10)
                    font.pixelSize: 14 * root.unit
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 8
                }
            }
        }

        OsdPanel {
            anchors.top: radioCard.bottom
            anchors.topMargin: 5 * root.unit
            anchors.right: radioCard.right
            width: timerReadout.implicitWidth + 20 * root.unit
            height: 27 * root.unit

            Readout {
                id: timerReadout
                objectName: "herelinkOsdTimer"
                anchors.fill: parent
                text: "◷  " + root.d.timer
                font.pixelSize: 14 * root.unit
                horizontalAlignment: Text.AlignHCenter
            }
        }

        OsdPanel {
            id: batteryCard
            objectName: "herelinkOsdBatteryCard"
            x: controlDeck.x
            y: controlDeck.y - height - 12 * root.unit
            width: 222 * root.unit; height: 36 * root.unit
            Row {
                id: batteryRow
                anchors.fill: parent; anchors.leftMargin: 10 * root.unit; anchors.rightMargin: 10 * root.unit
                spacing: 5 * root.unit
                ReadoutIcon { source: "qrc:/InstrumentValueIcons/battery-full.svg" }
                Readout {
                    width: 92 * root.unit; height: parent.height
                    readonly property real cellVoltage: Number(root.d.voltage)
                    readonly property color voltageColor: !Number.isFinite(cellVoltage) ? "#aab8c0"
                        : cellVoltage < 3.40 ? root.danger
                        : cellVoltage < 3.60 ? "#fb923c"
                        : cellVoltage < 3.70 ? "#facc15" : "#22c55e"
                    textFormat: Text.StyledText
                    text: "<font color=\"" + voltageColor + "\">" + root.d.voltage
                        + "</font> " + root.d.voltageUnit
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 8
                }
                Rectangle { width: 1; height: 18 * root.unit; anchors.verticalCenter: parent.verticalCenter; color: "#72828a" }
                ReadoutIcon { source: "qrc:/InstrumentValueIcons/bolt.svg" }
                Readout {
                    width: batteryRow.width - 124 * root.unit - 4 * batteryRow.spacing - 1
                    height: parent.height
                    text: root.d.current + " A"
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 8
                }
            }
        }

        OsdPanel {
            id: controlDeck
            objectName: "herelinkOsdControlDeck"
            x: root.leftReserve + root.gap
            y: root.height - root.bottomReserve - height - 12.5 * root.unit
            width: 270 * root.unit
            height: 132 * root.unit

            Row {
                id: sticksCard
                x: 13 * root.unit; y: 20 * root.unit
                height: 75 * root.unit; spacing: 10 * root.unit
                OsdStick { label: "L"; width: 51 * root.unit; height: parent.height; available: root.d.sticks; horizontal: root.d.yaw; vertical: root.d.throttle }
                OsdStick { label: "R"; width: 51 * root.unit; height: parent.height; available: root.d.sticks; horizontal: root.d.roll; vertical: root.d.pitch }
            }
            Rectangle {
                x: 137 * root.unit; y: 14 * root.unit
                width: 1; height: 104 * root.unit
                color: "#aab8c0"
            }
            CameraTiltIndicator {
                objectName: "herelinkOsdCameraTilt"
                x: 145 * root.unit; y: 8 * root.unit
                width: 112 * root.unit; height: 116 * root.unit
                available: root.d.cameraTiltAvailable === true
                degrees: Number(root.d.cameraTiltDegrees)
                camera: root.d.camera
                requestedCamera: root.d.cameraRequested ?? 0
                accent: root.accent
            }
        }

        OsdPanel {
            id: attitudeCard
            objectName: "herelinkOsdAttitudeCard"
            x: root.width - root.rightReserve - width - root.gap
            y: root.height - root.bottomReserve - height - 12.5 * root.unit
            width: 309 * root.unit
            height: 132 * root.unit
            Rectangle {
                id: armingBadge
                objectName: "herelinkOsdArmingBadge"
                x: 8 * root.unit; y: 5 * root.unit
                width: 88 * root.unit; height: 26 * root.unit
                z: 1
                radius: 7 * root.unit
                readonly property bool disarmed: root.d.connected && !root.d.armed
                property real pulseDarkening: 1
                color: !root.d.connected ? "#99131e2c" : disarmed ? "white" : Qt.darker("#c00000", pulseDarkening)

                SequentialAnimation on pulseDarkening {
                    running: armingBadge.visible && root.d.connected && root.d.armed
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: 1
                        to: 1.5
                        duration: 800
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        from: 1.5
                        to: 1
                        duration: 800
                        easing.type: Easing.InOutSine
                    }
                }

                Readout {
                    anchors.fill: parent; anchors.leftMargin: 5 * root.unit; anchors.rightMargin: 5 * root.unit
                    text: !root.d.connected ? qsTr("NO VEHICLE") : armingBadge.disarmed ? qsTr("DISARMED") : qsTr("ARMED")
                    color: armingBadge.disarmed ? "black" : "white"
                    font.pixelSize: 14 * root.unit
                    horizontalAlignment: Text.AlignHCenter
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 8
                }
            }
            Rectangle {
                x: 150 * root.unit; y: 14 * root.unit
                width: 1; height: 104 * root.unit
                color: "#aab8c0"
            }
            Loader {
                id: attitudeLoader
                objectName: "herelinkOsdAttitudeLoader"
                x: 8 * root.unit; y: 8 * root.unit
                width: 140 * root.unit; height: 116 * root.unit
                readonly property string hiddenReason: !root.visible || !graphics.visible ? "OSD hidden"
                    : !root.backend.attitudeAvailable ? "attitude unavailable"
                    : QGroundControl.settingsManager.appSettings.virtualJoystick.rawValue ? "native virtual joystick"
                    : root.attitudeOverlapsPip() ? "expanded PiP overlap" : ""
                active: hiddenReason.length === 0
                visible: active; enabled: false
                sourceComponent: attitudeComponent
                onHiddenReasonChanged: console.info("Herelink 3D attitude:", hiddenReason || "visible")
            }
            Readout {
                x: attitudeLoader.x; y: attitudeLoader.y
                width: attitudeLoader.width; height: attitudeLoader.height
                visible: !attitudeLoader.active
                text: qsTr("Attitude —"); color: "#aab8c0"
                font.pixelSize: 14 * root.unit
                horizontalAlignment: Text.AlignHCenter
            }
            Column {
                objectName: "herelinkOsdTelemetry"
                x: 156 * root.unit
                y: 11 * root.unit
                width: 140 * root.unit; spacing: 2 * root.unit
                Row {
                    width: parent.width; height: 18 * root.unit
                    ReadoutIcon { source: "qrc:/Herelink/roll.svg" }
                    Readout { width: 45 * root.unit; height: parent.height; text: "Roll"; font.pixelSize: 14 * root.unit }
                    Readout { width: parent.width - 61 * root.unit; height: parent.height; text: root.backend.attitudeAvailable ? root.backend.attitudeRoll.toFixed(1) + "°" : "—"; color: root.accent; font.pixelSize: 14 * root.unit; horizontalAlignment: Text.AlignRight }
                }
                Row {
                    width: parent.width; height: 18 * root.unit
                    ReadoutIcon { source: "qrc:/Herelink/pitch.svg" }
                    Readout { width: 45 * root.unit; height: parent.height; text: "Pitch"; font.pixelSize: 14 * root.unit }
                    Readout { width: parent.width - 61 * root.unit; height: parent.height; text: root.backend.attitudeAvailable ? root.backend.attitudePitch.toFixed(1) + "°" : "—"; color: root.accent; font.pixelSize: 14 * root.unit; horizontalAlignment: Text.AlignRight }
                }
                Rectangle { width: parent.width; height: 1; color: "#72828a" }
                Readout { width: parent.width; height: 18 * root.unit; text: "△ Alt  " + root.d.altitude + " m"; font.pixelSize: 14 * root.unit }
                Readout { width: parent.width; height: 18 * root.unit; text: "↕ Climb  " + root.d.climb + " m/s"; font.pixelSize: 14 * root.unit }
                Readout { width: parent.width; height: 18 * root.unit; text: "Throttle  " + root.d.motorOutput + " %"; font.pixelSize: 14 * root.unit }
            }
        }

        OsdPanel {
            id: banner
            visible: root.d.banner.length > 0
            x: Math.max((root.width - width) / 2, controlDeck.x + controlDeck.width + root.gap)
            y: root.height - root.bottomReserve - height - 12.5 * root.unit
            width: 267 * root.unit; height: 36 * root.unit
            border.color: root.d.tone === "ok" ? root.accent : root.d.tone === "critical" ? root.danger : root.caution
            Readout {
                anchors.fill: parent; anchors.margins: 8 * root.unit
                text: root.d.banner; font.pixelSize: 16 * root.unit
                horizontalAlignment: Text.AlignHCenter
                fontSizeMode: Text.HorizontalFit; minimumPixelSize: 10
            }
        }

        Image {
            readonly property real scaleUnit: Math.max(0.55, Math.min(root.width / 1713, root.height / 918))
            anchors.centerIn: parent
            // The PNG's transparent padding leaves about 36 units of visible artwork.
            width: 57 * scaleUnit; height: width
            source: "qrc:/Herelink/reticle.png"
            mipmap: true
        }
    }

    Button {
        id: toggleButton
        objectName: "herelinkOsdToggle"
        x: attitudeCard.x + attitudeCard.width - width
        y: attitudeCard.y - height - root.gap
        text: root.backend.overlayVisible ? "OSD ●" : "OSD ○"
        Accessible.name: qsTr("Toggle custom OSD")
        Accessible.description: qsTr("Hold or right-click for OSD options")
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Hold or right-click for OSD options")
        checkable: true; checked: root.backend.overlayVisible
        width: Math.max(32, 38 * root.unit); height: Math.max(16, 18 * root.unit)
        background: OsdPanel { radius: height / 3; border.width: toggleButton.activeFocus ? 2 : 1 }
        contentItem: Text { text: toggleButton.text; color: "white"; font.pixelSize: Math.max(8, 10 * root.unit); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        onClicked: root.backend.overlayVisible = !root.backend.overlayVisible
        onPressAndHold: osdOptions.popup()
        TapHandler { acceptedButtons: Qt.RightButton; onTapped: osdOptions.popup() }
        Keys.onMenuPressed: osdOptions.popup()
        Menu {
            id: osdOptions
            MenuItem {
                text: qsTr("Herelink HDMI")
                enabled: root.backend.cameraProfileAvailable && root.backend.data.connected && !root.backend.data.armed
                checkable: true
                checked: root.backend.herelinkHdmi
                onTriggered: root.backend.herelinkHdmi = checked
            }
        }
    }
}
