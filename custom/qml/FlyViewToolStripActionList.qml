import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.Viewer3D

ToolStripActionList {
    id: _root
    objectName: "herelinkFlyViewToolStripActionList"

    signal displayPreFlightChecklist

    model: [
        Viewer3DShowAction { },
        PreFlightCheckListShowAction { onTriggered: displayPreFlightChecklist() },
        // Keep the standard Land action and confirmation, independently of Takeoff.
        GuidedActionLand {
            objectName: "herelinkLandAction"
            visible: _guidedController.showLand
        },
        GuidedActionPause { },
        FlyViewAdditionalActionsButton { },
        FlyViewGripperButton { }
    ]
}
