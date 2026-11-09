#pragma once
#include "UnitTest.h"

class HerelinkOsdTest : public UnitTest
{
    Q_OBJECT
private slots:
    void disconnectedAndSettings();
    void timerPolicy();
    void telemetryFreshness();
    void batteryAndCamera();
    void joystickSnapshots();
};
