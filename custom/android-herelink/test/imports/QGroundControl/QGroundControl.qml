pragma Singleton
import QtQuick

QtObject {
    property QtObject multiVehicleManager: QtObject { property var activeVehicle: null }
    property QtObject videoManager: QtObject { property var videoStreamControl: null }
    property QtObject settingsManager: QtObject {
        property QtObject appSettings: QtObject {
            property QtObject virtualJoystick: QtObject { property bool rawValue: false }
        }
    }
}
