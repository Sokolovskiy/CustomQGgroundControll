#include "HerelinkFlyViewTest.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>

#include "ToolStripActionList.h"
#include "MockLink.h"
#include "Vehicle.h"
#include "HerelinkOsd.h"
#include <QtCore/QDir>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickItemGrabResult>

UT_REGISTER_TEST(HerelinkFlyViewTest, TestLabel::Integration)

namespace {
QQuickItem *visibleText(QQuickItem *item, const QString &text)
{
    if (!item || !item->isVisible()) {
        return nullptr;
    }
    if (item->property("text").toString() == text) {
        return item;
    }
    for (QQuickItem *child : item->childItems()) {
        if (auto *found = visibleText(child, text)) {
            return found;
        }
    }
    return nullptr;
}
}

void HerelinkFlyViewTest::toolstripPolicy()
{
    // Exercise the actual custom engine selector and complete Fly View without
    // a vehicle. Never trigger an action or send any vehicle commands.
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }
    QVERIFY(_rootItem);
    QVERIFY(!_engine->rootObjects().isEmpty());
    auto *const actions = _engine->rootObjects().first()->findChild<ToolStripActionList *>(
        QStringLiteral("herelinkFlyViewToolStripActionList"));
    QVERIFY2(actions, "The Herelink resource override did not replace the toolstrip list");
    QVERIFY(_engine->rootObjects().first()->findChild<QQuickItem *>(QStringLiteral("herelinkOsdView")));
    auto model = actions->model();
    QCOMPARE(model.count(&model), 6);
    QObject *const land = actions->findChild<QObject *>(QStringLiteral("herelinkLandAction"));
    QVERIFY(land);

    // Verify Land stays eligible even in the state where upstream would show
    // Takeoff instead. The standard guided controller still decides eligibility.
    QQmlEngine engine;
    QQmlComponent controllerComponent(&engine);
    controllerComponent.setData(R"(
        import QtQml
        QtObject {
            property bool showLand: false
            property bool showTakeoff: true
            property string landTitle: "Land"
            property string landMessage: "Land here"
            property int actionLand: 2
        }
    )", QUrl());
    QScopedPointer<QObject> controller(controllerComponent.create());
    QVERIFY2(controller, qPrintable(controllerComponent.errorString()));
    QVERIFY(land->setProperty("_guidedController", QVariant::fromValue(controller.data())));
    QCOMPARE(land->property("visible").toBool(), false);
    QVERIFY(controller->setProperty("showLand", true));
    QCOMPARE(land->property("visible").toBool(), true);
    QCOMPARE(land->property("enabled").toBool(), true);
    QCOMPARE(land->property("actionID").toInt(), 2);
    for (qsizetype i = 0; i < model.count(&model); ++i) {
        QObject *const action = model.at(&model, i);
        QVERIFY(action);
        if (action->property("actionID").isValid()) {
            const int actionId = action->property("actionID").toInt();
            QVERIFY(actionId != 1); // RTL
            QVERIFY(actionId != 3); // Takeoff
        }
    }
    // Destroy the UI before its temporary controller.
    stopUI();
}

void HerelinkFlyViewTest::readinessControls()
{
    // MockLink does not acknowledge these optional stream interval requests.
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Giving up sending command after max retries: MAV_CMD_SET_MESSAGE_INTERVAL")));
    runWithMockLink([] { return MockLink::startAPMArduCopterMockLink(); },
                    [this](QPointer<MockLink>, Vehicle *vehicle) {
        QVERIFY(vehicle);
        QVERIFY(!vehicle->armed());
        QQuickItem *const status = findVisibleItem(_rootItem, QStringLiteral("toolbar_mainStatusIndicator"));
        QVERIFY(status);
        QQuickItem *const mode = findVisibleItem(_rootItem, QStringLiteral("toolbar_flightModeIndicator"));
        QVERIFY(mode);
        QVERIFY(visibleText(mode, vehicle->flightMode()));
        QVERIFY(QMetaObject::invokeMethod(status, "dropMainStatusIndicator"));
        QTRY_VERIFY_WITH_TIMEOUT(visibleText(_rootItem, QStringLiteral("Arm")), 5000);

        // Change presentation state only; never arm even the mock vehicle.
        QVERIFY(status->setProperty("_armed", true));
        QTRY_VERIFY_WITH_TIMEOUT(visibleText(_rootItem, QStringLiteral("Disarm")), 2000);
        QVERIFY(!vehicle->armed());
        QVERIFY(status->setProperty("_armed", false));
        QVERIFY(QMetaObject::invokeMethod(_engine->rootObjects().first(), "closeIndicatorDrawer"));
        QTRY_VERIFY_WITH_TIMEOUT(!visibleText(_rootItem, QStringLiteral("Arm")), 2000);
        auto *const osd = qobject_cast<HerelinkOsd *>(
            _engine->rootContext()->contextProperty(QStringLiteral("herelinkOsd")).value<QObject *>());
        QVERIFY(osd);
        const bool wasVisible = osd->overlayVisible();
        osd->setOverlayVisible(false);
        vehicle->setPrearmError(QStringLiteral("OSD test PreArm warning"));
        QTRY_VERIFY_WITH_TIMEOUT(visibleText(_rootItem, QStringLiteral("OSD test PreArm warning")), 3000);
        vehicle->setPrearmError(QString());
        osd->setOverlayVisible(wasVisible);
    });
}

void HerelinkFlyViewTest::osdLayout()
{
    // Standalone presentation fixture: no fabricated telemetry or reference
    // background. Screenshots show the disconnected overlay over charcoal.
    HerelinkOsd osd;
    const bool oldVisible = osd.overlayVisible();
    osd.setOverlayVisible(true);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("herelinkOsd"), &osd);
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/Herelink/HerelinkOsdView.qml")));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *const item = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(item);
    QQmlComponent fixtureComponent(&engine);
    fixtureComponent.setData(R"(
        import QtQuick
        Rectangle {
            color: "#334755"
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * 0.59
                text: "LAYOUT TEST — NO VEHICLE / NO VIDEO"
                color: "#a2b1bf"
                font.pixelSize: 16
            }
        }
    )", QUrl());
    QScopedPointer<QObject> fixture(fixtureComponent.create());
    QVERIFY2(fixture, qPrintable(fixtureComponent.errorString()));
    auto *const fixtureItem = qobject_cast<QQuickItem *>(fixture.data());
    QVERIFY(fixtureItem);
    QQuickWindow window;
    window.setColor(QColor(QStringLiteral("#334755")));
    window.setPosition(0, 0);
    window.resize(1000, 700);
    fixtureItem->setParentItem(window.contentItem());
    item->setParentItem(fixtureItem);
    window.show();
    QDir().mkpath(QStringLiteral("osd-screenshots"));
    for (const QSize size : {QSize(1920, 1080), QSize(1280, 720), QSize(960, 640)}) {
        item->setSize(size);
        fixtureItem->setSize(size);
        QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 5000);
        // Render the complete layout item, independent of desktop DPI/window
        // size limits. These are presentation fixture captures, not live video.
        auto capture = fixtureItem->grabToImage(size);
        QVERIFY(capture);
        QTRY_VERIFY_WITH_TIMEOUT(!capture->image().isNull(), 5000);
        const QImage screenshot = capture->image();
        QCOMPARE(screenshot.size(), size * window.devicePixelRatio());
        QVERIFY(!screenshot.isNull());
        // Normalize screen density for portable, exact-size comparison images.
        QVERIFY(screenshot.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(
            QStringLiteral("osd-screenshots/disconnected-%1x%2.png").arg(size.width()).arg(size.height())));
    }
    QObject *const graphics = object->findChild<QObject *>(QStringLiteral("herelinkOsdGraphics"));
    QObject *const toggle = object->findChild<QObject *>(QStringLiteral("herelinkOsdToggle"));
    QVERIFY(graphics);
    QVERIFY(toggle);
    osd.setOverlayVisible(false);
    QCOMPARE(graphics->property("visible").toBool(), false);
    QCOMPARE(toggle->property("visible").toBool(), true);
    osd.setOverlayVisible(oldVisible);
    window.hide();
    item->setParentItem(nullptr);
    fixtureItem->setParentItem(nullptr);
}

void HerelinkFlyViewTest::osdEdgeAnchors()
{
    // Reproduce the real Fly View integration, including large stale insets
    // from collapsed PiP and a hidden multi-vehicle panel. The standalone
    // presentation capture alone would not catch the original centering bug.
    HerelinkOsd osd;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("herelinkOsd"), &osd);
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/Herelink/FlyViewCustomLayer.qml")));
    const QVariantMap cornerInsets{
        {QStringLiteral("leftEdgeTopInset"), 64},
        {QStringLiteral("leftEdgeBottomInset"), 400},
        {QStringLiteral("rightEdgeTopInset"), 360},
        {QStringLiteral("rightEdgeCenterInset"), 360},
        {QStringLiteral("bottomEdgeLeftInset"), 240},
        {QStringLiteral("bottomEdgeRightInset"), 200}
    };
    const QVariantMap mapControl{
        {QStringLiteral("pipState"), QVariantMap{
            {QStringLiteral("state"), QStringLiteral("pip")},
            {QStringLiteral("fullState"), QStringLiteral("full")}
        }}
    };
    QScopedPointer<QObject> layer(component.createWithInitialProperties({
        {QStringLiteral("width"), 1280}, {QStringLiteral("height"), 720},
        {QStringLiteral("parentToolInsets"), cornerInsets},
        {QStringLiteral("mapControl"), mapControl}
    }));
    QVERIFY2(layer, qPrintable(component.errorString()));
    auto *const view = layer->findChild<QQuickItem *>(QStringLiteral("herelinkOsdView"));
    auto *const state = layer->findChild<QQuickItem *>(QStringLiteral("herelinkOsdStateCard"));
    auto *const battery = layer->findChild<QQuickItem *>(QStringLiteral("herelinkOsdBatteryCard"));
    auto *const camera = layer->findChild<QQuickItem *>(QStringLiteral("herelinkOsdCameraCard"));
    QVERIFY(view && state && battery && camera);
    for (const QSize size : {QSize(1920, 1080), QSize(1280, 720), QSize(960, 640)}) {
        QVERIFY(layer->setProperty("width", size.width()));
        QVERIFY(layer->setProperty("height", size.height()));
        const qreal gap = view->property("gap").toReal();
        QVERIFY(qAbs(state->x() - gap) < 0.5);
        QVERIFY(qAbs(battery->x() + battery->width() - (view->width() - gap)) < 0.5);
        QVERIFY(qAbs(camera->x() - gap) < 0.5);
        QVERIFY(qAbs(camera->y() + camera->height() - (view->height() - gap)) < 0.5);
    }
}
