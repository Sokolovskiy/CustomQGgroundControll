import QtQuick
import Fatum.Display 1.0

// Facts and native video state are read-only inputs. This object sends no commands.
QtObject {
    id: root

    property var vehicle: null
    property var videoControl: null
    property bool virtualJoystick: false
    property bool overlayVisible: true
    property int cellCount: 12
    property real yawReference: NaN
    property NativeTelemetry telemetry: NativeTelemetry { yawReference: root.yawReference }
    readonly property string callsign: ""
    readonly property bool connected: vehicle !== null && !vehicle.vehicleLinkManager.communicationLost
    readonly property real attitudeRoll: connected ? Number(vehicle.roll.rawValue) : NaN
    readonly property real attitudePitch: connected ? Number(vehicle.pitch.rawValue) : NaN
    readonly property real attitudeHeading: connected ? Number(vehicle.heading.rawValue) : NaN
    readonly property bool attitudeAvailable: connected && telemetry.attitudeAvailable
        && Number.isFinite(attitudeRoll) && Number.isFinite(attitudePitch) && Number.isFinite(attitudeHeading)
    onVehicleChanged: {
        yawReference = NaN
        syncYawReference()
    }
    onAttitudeAvailableChanged: syncYawReference()
    onAttitudeHeadingChanged: syncYawReference()
    readonly property var battery: connected && vehicle.batteries.count > 0 ? vehicle.batteries.get(0) : null
    readonly property real voltage: battery ? Number(battery.voltage.rawValue) : NaN
    readonly property real current: battery ? Number(battery.current.rawValue) : NaN
    readonly property real altitude: connected ? Number(vehicle.altitudeRelative.rawValue) : NaN
    readonly property real climb: connected ? Number(vehicle.climbRate.rawValue) : NaN
    readonly property real throttle: connected ? Number(vehicle.throttlePct.rawValue) : NaN
    readonly property var flightTime: connected ? vehicle.getFact("flightTime") : null
    readonly property real seconds: flightTime ? Number(flightTime.rawValue) : NaN
    readonly property string timerText: Number.isFinite(seconds)
        ? Math.floor(seconds / 60).toString().padStart(2, "0") + ":"
          + Math.floor(seconds % 60).toString().padStart(2, "0") : "—"
    readonly property var data: ({
        connected: connected,
        armed: connected && vehicle.armed,
        mode: connected ? vehicle.flightMode : qsTr("NO VEHICLE"),
        voltage: number(voltage > 0 && cellCount > 0 ? voltage / cellCount : NaN, 2),
        voltageUnit: "V/cell",
        current: number(current >= 0 ? current : NaN, 1),
        altitude: number(altitude, 1),
        climb: number(climb, 1),
        motorOutput: number(throttle, 0),
        radioRssi: connected ? telemetry.radioRssi : "—",
        radioSnr: connected ? telemetry.radioSnr : "—",
        sticks: telemetry.channelsAvailable && [yaw, stickThrottle, roll, pitch].every(Number.isFinite),
        yaw: yaw, throttle: stickThrottle, roll: roll, pitch: pitch,
        cameraTiltAvailable: false, cameraTiltDegrees: NaN, cameraRequested: 0,
        camera: connected && videoControl && videoControl.cameraInfoReceived
            ? (videoControl.currentActiveSource < 2
               ? "CAM" + (videoControl.currentHdmiInput + 1) : "RTSP") : "CAM —",
        timer: timerText,
        banner: connected ? "" : qsTr("NO VEHICLE"),
        tone: connected ? "ok" : "warning"
    })
    readonly property real yaw: axis("RCMAP_YAW")
    readonly property real stickThrottle: axis("RCMAP_THROTTLE")
    readonly property real roll: axis("RCMAP_ROLL")
    readonly property real pitch: -axis("RCMAP_PITCH")

    function axis(mapping: string): real {
        // Read the notifications as dependencies before calling the native reader.
        if (!connected || !vehicle.parameterManager.parametersReady || !telemetry.channelsAvailable
            || telemetry.channels.length === 0)
            return NaN
        return telemetry.axis(mapping)
    }

    function syncYawReference() {
        if (!attitudeAvailable)
            yawReference = NaN
        else if (!Number.isFinite(yawReference))
            yawReference = attitudeHeading
    }

    // NED Z-Y-X attitude in scene axes (+X right, +Y up, -Z nose).
    // Preserve the first valid heading as the fixed scene's yaw reference.
    readonly property quaternion attitudeRotation: {
        if (!attitudeAvailable || !Number.isFinite(yawReference))
            return Qt.quaternion(1, 0, 0, 0)
        const r = attitudeRoll * Math.PI / 360
        const p = attitudePitch * Math.PI / 360
        const y = (attitudeHeading - yawReference) * Math.PI / 360
        const cr = Math.cos(r), sr = Math.sin(r)
        const cp = Math.cos(p), sp = Math.sin(p)
        const cy = Math.cos(y), sy = Math.sin(y)
        return Qt.quaternion(cy * cp * cr + sy * sp * sr,
                             cy * sp * cr + sy * cp * sr,
                             cy * sp * sr - sy * cp * cr,
                             -cy * cp * sr + sy * sp * cr)
    }
    readonly property var attitudeVectors: {
        const motors = []
        for (let i = 0; i < 4; ++i) {
            const x = i % 2 === 0 ? -65.54 : 65.54
            const z = i < 2 ? -65.54 : 65.54
            const position = rotate(Qt.vector3d(x, 6.48, z))
            const arm = rotate(Qt.vector3d(x, 0, z))
            const length = Math.sqrt(arm.x * arm.x + arm.z * arm.z)
            motors.push({
                lower: attitudeAvailable && Math.hypot(attitudeRoll, attitudePitch) > 1 && arm.y < -0.05,
                position: position.plus(Qt.vector3d(length > 0 ? arm.x * 24 / length : 0,
                                                   12, length > 0 ? arm.z * 24 / length : 0))
            })
        }
        const motion = telemetry.motion
        return { motorTilts: motors, motionAvailable: motion.available, motionVisible: motion.visible,
                 motionRotation: motion.rotation, motionSpeed: motion.speed, motionAxes: motion.axes }
    }

    function number(value: real, decimals: int): string {
        return Number.isFinite(value) ? value.toFixed(decimals) : "—"
    }

    function rotate(v: vector3d): vector3d {
        const q = attitudeRotation
        const u = Qt.vector3d(q.x, q.y, q.z)
        return v.plus(u.crossProduct(v).times(2 * q.scalar))
                .plus(u.crossProduct(u.crossProduct(v)).times(2))
    }
}
