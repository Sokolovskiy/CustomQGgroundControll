import QtQuick
import QtQuick3D

Node {
    id: root
    required property quaternion directionRotation
    required property color arrowColor
    property real arrowLength: 70
    rotation: directionRotation

    // Local -Z is the arrow direction; the backend supplies the scene rotation.
    PrincipledMaterial {
        id: arrowMaterial
        baseColor: root.arrowColor
        lighting: PrincipledMaterial.NoLighting
    }
    Model {
        source: "#Cube"
        z: -root.arrowLength * 0.375
        scale: Qt.vector3d(0.035, 0.035, root.arrowLength * 0.0075)
        materials: arrowMaterial
    }
    Model {
        source: "#Cone"
        z: -root.arrowLength * 0.875
        eulerRotation.x: -90
        scale: Qt.vector3d(root.arrowLength * 0.0018, root.arrowLength * 0.0025, root.arrowLength * 0.0018)
        materials: arrowMaterial
    }
}
