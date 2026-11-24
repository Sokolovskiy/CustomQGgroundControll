import QtQuick
import QtTest
import "qrc:/Herelink"

TestCase {
    id: test
    name: "HerelinkDisplay"
    when: windowShown
    width: 1280
    height: 600
    visible: true

    QtObject {
        id: battery
        property QtObject voltage: QtObject { property real rawValue: 44.4 }
        property QtObject current: QtObject { property real rawValue: 12.5 }
    }
    QtObject {
        id: vehicle
        property QtObject vehicleLinkManager: QtObject { property bool communicationLost: false }
        property QtObject parameterManager: QtObject { property bool parametersReady: true }
        property QtObject roll: QtObject { property real rawValue: 12 }
        property QtObject pitch: QtObject { property real rawValue: -8 }
        property QtObject heading: QtObject { property real rawValue: 0 }
        property QtObject altitudeRelative: QtObject { property real rawValue: 123.4 }
        property QtObject climbRate: QtObject { property real rawValue: 1.2 }
        property QtObject throttlePct: QtObject { property real rawValue: 42 }
        property QtObject flightTime: QtObject { property real rawValue: 125 }
        property QtObject batteries: QtObject {
            property int count: 1
            function get(index: int): var { return battery }
        }
        property bool armed: false
        property string flightMode: "Position Hold"
        function getFact(name: string): var { return flightTime }
    }
    QtObject {
        id: video
        property bool cameraInfoReceived: true
        property int currentHdmiInput: 0
        property int currentActiveSource: 0
    }
    NativeOsdModel { id: model; vehicle: null; videoControl: video }
    Rectangle { anchors.fill: parent; color: "#183128" }
    HerelinkOsdView { id: display; anchors.fill: parent; backend: model }

    function init() {
        model.vehicle = null
        vehicle.roll.rawValue = 12
        vehicle.pitch.rawValue = -8
        vehicle.heading.rawValue = 0
        model.vehicle = vehicle
        vehicle.vehicleLinkManager.communicationLost = false
        model.telemetry.channelsAvailable = true
        model.telemetry.attitudeAvailable = true
        vehicle.armed = false
        video.cameraInfoReceived = true
        model.virtualJoystick = false
    }

    function test_disconnectedAndReconnect() {
        model.vehicle = null
        verify(!model.data.connected)
        compare(model.data.voltage, "—")
        verify(!model.attitudeAvailable)
        compare(model.data.camera, "CAM —")
        model.vehicle = vehicle
        compare(model.data.voltage, "3.70")
        verify(model.attitudeAvailable)
        vehicle.vehicleLinkManager.communicationLost = true
        verify(!model.data.connected)
        compare(model.data.radioRssi, "—")
        verify(!model.attitudeAvailable)
    }

    function test_nativeFactsAndNoCommands() {
        compare(model.data.current, "12.5")
        compare(model.data.altitude, "123.4")
        compare(model.data.motorOutput, "42")
        compare(model.data.timer, "02:05")
        compare(model.data.mode, vehicle.flightMode)
        compare(model.data.camera, "CAM1")
        verify(model.data.sticks)
        model.telemetry.channelsAvailable = false
        verify(!model.data.sticks)
        model.telemetry.attitudeAvailable = false
        verify(!model.attitudeAvailable)
        // The fake vehicle deliberately has no command or parameter-write API.
        model.overlayVisible = false
        compare(vehicle.armed, false)
        compare(vehicle.flightMode, "Position Hold")
        model.overlayVisible = true
    }

    function test_visual() {
        waitForRendering(display)
        const loader = findChild(display, "herelinkOsdAttitudeLoader")
        verify(loader.active, loader.hiddenReason)
        verify(findChild(display, "herelinkOsdAttitudeLoader").item !== null)
        const image = grabImage(display)
        verify(image.width > 0)
        image.save(".cache/herelink-android-preview.png")
    }

    function test_nativeModesAndUnknownCamera() {
        vehicle.flightMode = "Acro"
        compare(model.data.mode, "Acro")
        video.cameraInfoReceived = false
        compare(model.data.camera, "CAM —")
        compare(model.data.cameraTiltAvailable, false)
        vehicle.flightMode = "Position Hold"
    }

    function test_interceptedStockLayerUrl() {
        const component = Qt.createComponent("qrc:/qml/QGroundControl/FlightDisplay/FlyViewCustomLayer.qml")
        compare(component.status, Component.Ready, component.errorString())
        const layer = component.createObject(test)
        verify(layer !== null, component.errorString())
        layer.destroy()
    }

    function test_cornerInsetsDoNotShiftOsd() {
        const component = Qt.createComponent("qrc:/qml/QGroundControl/FlightDisplay/FlyViewCustomLayer.qml")
        compare(component.status, Component.Ready, component.errorString())
        const layer = component.createObject(test, {
            width: 1280, height: 720,
            parentToolInsets: { leftEdgeCenterInset: 100, rightEdgeCenterInset: 400,
                                bottomEdgeLeftInset: 250, bottomEdgeRightInset: 200 },
            mapControl: { pipState: { state: "pip", fullState: "full" }, pipView: null }
        })
        verify(layer !== null, component.errorString())
        const deck = findChild(layer, "herelinkOsdControlDeck")
        const attitude = findChild(layer, "herelinkOsdAttitudeCard")
        verify(deck.x < 20)
        verify(1280 - attitude.x - attitude.width < 20)
        verify(720 - deck.y - deck.height < 20)
        layer.destroy()
    }

    function test_virtualJoystickSpace() {
        model.virtualJoystick = true
        const loader = findChild(display, "herelinkOsdAttitudeLoader")
        compare(loader.active, false)
        compare(loader.hiddenReason, "native virtual joystick")
        model.virtualJoystick = false
        tryCompare(loader, "active", true)
    }

    function test_yawUsesInitialHeading() {
        model.vehicle = null
        vehicle.roll.rawValue = 0
        vehicle.pitch.rawValue = 0
        vehicle.heading.rawValue = 120
        model.vehicle = vehicle
        compare(model.yawReference, 120)
        let nose = model.rotate(Qt.vector3d(0, 0, -1))
        fuzzyCompare(nose.z, -1, 0.00001)
        vehicle.heading.rawValue = 210
        nose = model.rotate(Qt.vector3d(0, 0, -1))
        fuzzyCompare(nose.x, 1, 0.00001)
        fuzzyCompare(nose.z, 0, 0.00001)
        compare(model.telemetry.yawReference, 120)
        vehicle.roll.rawValue = 15
        vehicle.pitch.rawValue = 10
        verify(model.rotate(Qt.vector3d(1, 0, 0)).y < 0)
        verify(model.rotate(Qt.vector3d(0, 0, -1)).y > 0)
    }

    function test_headingWrapAndReferenceReset() {
        model.vehicle = null
        vehicle.roll.rawValue = 0
        vehicle.pitch.rawValue = 0
        vehicle.heading.rawValue = 359
        model.vehicle = vehicle
        vehicle.heading.rawValue = 1
        const nose = model.rotate(Qt.vector3d(0, 0, -1))
        fuzzyCompare(nose.x, Math.sin(2 * Math.PI / 180), 0.00001)
        model.telemetry.attitudeAvailable = false
        verify(!Number.isFinite(model.yawReference))
        vehicle.heading.rawValue = 95
        model.telemetry.attitudeAvailable = true
        compare(model.yawReference, 95)
        fuzzyCompare(model.rotate(Qt.vector3d(0, 0, -1)).z, -1, 0.00001)
    }
}
