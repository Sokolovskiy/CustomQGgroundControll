#include <QtQuickTest/quicktest.h>

#include "../DisplayResources.h"

class DisplayTestSetup : public QObject
{
    Q_OBJECT

public slots:

    void qmlEngineAvailable(QQmlEngine* engine) { engine->addUrlInterceptor(new DisplayResources(engine)); }
};

QUICK_TEST_MAIN_WITH_SETUP(herelink_display, DisplayTestSetup)

#include "main.moc"
