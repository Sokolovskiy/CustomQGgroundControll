#pragma once

#include "VehicleTest.h"

class HerelinkJoystickTest : public VehicleTest
{
    Q_OBJECT

private slots:
    void auxiliaryOverrideWireFormat();
    void profilePersistence();
};
