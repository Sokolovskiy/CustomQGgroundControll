pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

OsdPanel {
    id: root

    required property quaternion attitude
    required property real rollDegrees
    required property real pitchDegrees
    required property var vectors
    property bool embedded: false
    color: embedded ? "transparent" : "#99131e2c"
    border.width: embedded ? 0 : 1
    property color accentColor: "#ce9800"
    property color motionColor: "#55c8ff"
    property real readoutHeight: 72
    implicitWidth: 400
    implicitHeight: implicitWidth + readoutHeight
    enabled: false

    // Model axes: +X right, +Y up, -Z nose. No geographical position or animation.
    View3D {
        anchors.fill: parent
        anchors.bottomMargin: root.readoutHeight
        renderMode: View3D.Offscreen
        camera: fixedCamera
        // Bound the offscreen target on high-DPI displays as well.
        explicitTextureWidth: Math.min(880, Math.ceil(width * 2))
        explicitTextureHeight: Math.min(880, Math.ceil(height * 2))
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.NoAA
        }

        Node { id: sceneCenter; y: 6 }
        PerspectiveCamera {
            id: fixedCamera
            position: Qt.vector3d(228, 132, -290)
            lookAtNode: sceneCenter
            fieldOfView: 35
            clipNear: 10
            clipFar: 700
        }
        DirectionalLight {
            eulerRotation: Qt.vector3d(-35, -30, 0)
            brightness: 1.2
            ambientColor: "#666666"
            castsShadow: false
        }

        // A fixed horizontal reference stays independent of aircraft rotation.
        PrincipledMaterial {
            id: referenceMaterial
            baseColor: "#aab8c0"
            lighting: PrincipledMaterial.NoLighting
            opacity: 0.65
        }
        PrincipledMaterial {
            id: gridMaterial
            baseColor: referenceMaterial.baseColor
            lighting: PrincipledMaterial.NoLighting
            opacity: 0.42
        }
        Repeater3D {
            id: referenceGrid
            readonly property int cellsPerAxis: 5
            readonly property int linesPerAxis: cellsPerAxis + 1
            readonly property real halfExtent: 90
            model: 2 * linesPerAxis
            delegate: Model {
                required property int index
                readonly property bool runsAlongZ: index < referenceGrid.linesPerAxis
                readonly property int lineIndex: index % referenceGrid.linesPerAxis
                readonly property bool perimeter: lineIndex === 0 || lineIndex === referenceGrid.cellsPerAxis
                readonly property real offset: referenceGrid.halfExtent * (2 * lineIndex / referenceGrid.cellsPerAxis - 1)
                readonly property real thickness: perimeter ? 0.012 : 0.010
                readonly property real length: 2 * referenceGrid.halfExtent / 100
                source: "#Cube"
                y: -28
                x: runsAlongZ ? offset : 0
                z: runsAlongZ ? 0 : offset
                scale: runsAlongZ ? Qt.vector3d(thickness, thickness, length) : Qt.vector3d(length, thickness, thickness)
                materials: perimeter ? referenceMaterial : gridMaterial
            }
        }

        Repeater3D {
            model: 4
            delegate: AttitudeArrow3D {
                required property int index
                readonly property var marker: root.vectors.motorTilts[index]
                position: marker.position
                visible: marker.lower
                // Fixed scene down (-Y), independent of the aircraft's rotation.
                directionRotation: Qt.quaternion(Math.SQRT1_2, -Math.SQRT1_2, 0, 0)
                arrowColor: root.accentColor
                arrowLength: 32
            }
        }
        AttitudeArrow3D {
            y: -24
            visible: root.vectors.motionVisible
            directionRotation: root.vectors.motionRotation
            arrowColor: root.motionColor
            arrowLength: 95
        }

        Node {
            rotation: root.attitude
            scale: Qt.vector3d(1.13, 1.08, 1.13)

            PrincipledMaterial {
                id: frameMaterial
                baseColor: "#d4dde6"
                roughness: 0.85
                metalness: 0
            }
            PrincipledMaterial {
                id: darkMaterial
                baseColor: "#3d4753"
                roughness: 0.9
                metalness: 0
            }
            PrincipledMaterial {
                id: plateMaterial
                baseColor: "#2a3440"
                roughness: 0.85
            }
            PrincipledMaterial {
                id: accentMaterial
                baseColor: root.accentColor
                roughness: 0.8
                metalness: 0
            }
            PrincipledMaterial {
                id: hornMaterial
                baseColor: "#a6b2be"
                roughness: 0.75
            }
            PrincipledMaterial {
                id: hornRimMaterial
                baseColor: root.accentColor
                lighting: PrincipledMaterial.NoLighting
            }
            PrincipledMaterial {
                id: batteryMaterial
                baseColor: "#657383"
                roughness: 0.9
            }

            // Two diagonal arms form an X with four stationary motor/propeller pods.
            Model {
                source: "#Cube"
                scale: Qt.vector3d(1.64, 0.07, 0.12)
                eulerRotation.y: 45
                materials: frameMaterial
            }
            Model {
                source: "#Cube"
                scale: Qt.vector3d(1.64, 0.07, 0.12)
                eulerRotation.y: -45
                materials: frameMaterial
            }
            Model {
                source: "#Cube"
                y: 5
                scale: Qt.vector3d(0.46, 0.14, 0.62)
                materials: darkMaterial
            }
            Model {
                source: "#Cube"
                y: 12
                scale: Qt.vector3d(0.49, 0.025, 0.66)
                materials: plateMaterial
            }
            Model {
                source: "#Cube"
                y: -3.5
                scale: Qt.vector3d(0.48, 0.03, 0.64)
                materials: plateMaterial
            }
            Model {
                source: "#Cone"
                position: Qt.vector3d(0, 15, -20)
                eulerRotation.x: -90
                scale: Qt.vector3d(0.30, 0.38, 0.045)
                materials: accentMaterial
            }

            Repeater3D {
                model: 2
                delegate: Node {
                    required property int index
                    x: index === 0 ? -12.5 : 12.5
                    y: -13
                    Model {
                        source: "#Cube"
                        z: -6
                        scale: Qt.vector3d(0.19, 0.16, 0.62)
                        materials: batteryMaterial
                    }
                    Model {
                        source: "#Cube"
                        z: -38
                        scale: Qt.vector3d(0.19, 0.16, 0.045)
                        materials: frameMaterial
                    }
                    Repeater3D {
                        model: 2
                        delegate: Model {
                            required property int index
                            source: "#Cube"
                            z: index === 0 ? -24 : 9
                            scale: Qt.vector3d(0.206, 0.174, 0.06)
                            materials: frameMaterial
                        }
                    }
                }
            }

            Model {
                source: "#Cube"
                position: Qt.vector3d(0, 19, 5)
                scale: Qt.vector3d(0.34, 0.12, 0.36)
                materials: darkMaterial
            }
            Model {
                source: "#Cube"
                position: Qt.vector3d(0, 25, 5)
                scale: Qt.vector3d(0.36, 0.025, 0.38)
                materials: hornMaterial
            }
            Repeater3D {
                model: 3
                delegate: Model {
                    required property int index
                    source: "#Cube"
                    position: Qt.vector3d(17, 19, -4 + index * 9)
                    scale: Qt.vector3d(0.015, 0.06, 0.04)
                    materials: hornMaterial
                }
            }
            Model {
                source: "#Cube"
                position: Qt.vector3d(0, 29, 6)
                scale: Qt.vector3d(0.13, 0.085, 0.16)
                materials: darkMaterial
            }
            Node {
                position: Qt.vector3d(0, 39, 4)
                Model {
                    source: "#Cube"
                    z: 13
                    scale: Qt.vector3d(0.21, 0.14, 0.05)
                    materials: darkMaterial
                }
                Model {
                    materials: [hornMaterial, darkMaterial, hornRimMaterial]
                    geometry: ProceduralMesh {
                        id: hornMesh
                        readonly property var mesh: makeHorn()
                        positions: mesh.vertices
                        normals: mesh.normals
                        indexes: mesh.indices
                        subsets: [
                            ProceduralMeshSubset { offset: 0; count: hornMesh.mesh.shellEnd },
                            ProceduralMeshSubset {
                                offset: hornMesh.mesh.shellEnd
                                count: hornMesh.mesh.cavityEnd - offset
                            },
                            ProceduralMeshSubset {
                                offset: hornMesh.mesh.cavityEnd
                                count: hornMesh.mesh.indices.length - offset
                            }
                        ]

                        function makeHorn(): var {
                            const vertices = []
                            const normals = []
                            const indices = []

                            function ring(x, y, z) {
                                return [Qt.vector3d(-x, -y, z), Qt.vector3d(x, -y, z),
                                        Qt.vector3d(x, y, z), Qt.vector3d(-x, y, z)]
                            }

                            function quad(a, b, c, d) {
                                const first = vertices.length
                                const normal = b.minus(a).crossProduct(c.minus(a)).normalized()
                                vertices.push(a, b, c, d)
                                normals.push(normal, normal, normal, normal)
                                indices.push(first, first + 1, first + 2, first, first + 2, first + 3)
                            }

                            const mouth = ring(22, 13, -28)
                            const rimBack = ring(20.5, 12.5, -23)
                            const throat = ring(10, 7, 13)
                            const innerMouth = ring(18, 9, -28)
                            const innerThroat = ring(7, 4, 13)

                            for (let i = 0; i < 4; ++i) {
                                const next = (i + 1) % 4
                                quad(rimBack[i], rimBack[next], throat[next], throat[i])
                            }
                            const shellEnd = indices.length
                            for (let i = 0; i < 4; ++i) {
                                const next = (i + 1) % 4
                                quad(innerMouth[next], innerMouth[i], innerThroat[i], innerThroat[next])
                            }
                            const cavityEnd = indices.length
                            for (let i = 0; i < 4; ++i) {
                                const next = (i + 1) % 4
                                quad(mouth[i], mouth[next], rimBack[next], rimBack[i])
                                quad(mouth[next], mouth[i], innerMouth[i], innerMouth[next])
                            }
                            return { vertices, normals, indices, shellEnd, cavityEnd }
                        }
                    }
                }
            }

            Repeater3D {
                model: 4
                delegate: Node {
                    id: motorPod
                    required property int index
                    readonly property bool front: index < 2
                    x: index % 2 === 0 ? -58 : 58
                    z: front ? -58 : 58
                    Model {
                        source: "#Cylinder"
                        y: 6
                        scale: Qt.vector3d(0.20, 0.16, 0.20)
                        materials: darkMaterial
                    }
                    Model {
                        source: "#Cylinder"
                        y: 1
                        scale: Qt.vector3d(0.208, 0.035, 0.208)
                        materials: plateMaterial
                    }
                    Model {
                        source: "#Cylinder"
                        y: 12
                        visible: motorPod.front
                        scale: Qt.vector3d(0.215, 0.055, 0.215)
                        materials: accentMaterial
                    }
                    Model {
                        source: "#Cube"
                        y: 16
                        scale: Qt.vector3d(0.54, 0.025, 0.07)
                        eulerRotation.y: motorPod.index % 2 === 0 ? 20 : -20
                        materials: frameMaterial
                    }
                    Model {
                        source: "#Cylinder"
                        y: 18.5
                        scale: Qt.vector3d(0.072, 0.045, 0.072)
                        materials: hornMaterial
                    }
                }
            }
        }
    }

    Column {
        visible: !root.embedded
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        height: Math.max(0, root.readoutHeight - 16)
        spacing: 4
        Text {
            width: parent.width
            height: Math.max(0, (parent.height - parent.spacing) / 2)
            readonly property bool valid: Number.isFinite(root.rollDegrees) && Number.isFinite(root.pitchDegrees)
            readonly property string rollArrow: Math.abs(root.rollDegrees) <= 1 ? "·" : root.rollDegrees > 0 ? "→" : "←"
            readonly property string pitchArrow: Math.abs(root.pitchDegrees) <= 1 ? "·" : root.pitchDegrees > 0 ? "↑" : "↓"
            text: valid ? qsTr("Roll %1 %2°   Pitch %3 %4°").arg(rollArrow).arg(root.rollDegrees.toFixed(1))
                .arg(pitchArrow).arg(root.pitchDegrees.toFixed(1)) : "—"
            color: root.accentColor
            font.pixelSize: Math.max(14, Math.min(20, root.width * 0.05))
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            height: Math.max(0, (parent.height - parent.spacing) / 2)
            spacing: 12
            Text {
                text: qsTr("↓ Tilt")
                color: root.accentColor
                font.pixelSize: Math.max(13, Math.min(18, root.width * 0.045))
            }
            Text {
                text: root.vectors.motionAvailable
                    ? qsTr("➤ Motion %1 %2 m/s").arg(root.vectors.motionAxes).arg(root.vectors.motionSpeed.toFixed(1))
                    : qsTr("Motion —")
                color: root.motionColor
                font.pixelSize: Math.max(13, Math.min(18, root.width * 0.045))
            }
        }
    }
}
