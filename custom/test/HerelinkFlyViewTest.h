#pragma once

#include "QmlUITestBase.h"

class HerelinkFlyViewTest : public QmlUITestBase
{
    Q_OBJECT

private slots:
    void toolstripPolicy();
    void readinessControls();
    void osdLayout();
    void osdEdgeAnchors();
};
