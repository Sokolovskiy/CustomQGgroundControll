#include "HerelinkCorePlugin.h"
#include "HerelinkOsd.h"

#include <QtCore/QApplicationStatic>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlAbstractUrlInterceptor>
#include <QtQml/QQmlContext>

namespace {
class HerelinkResourceInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    QUrl intercept(const QUrl &url, DataType type) override
    {
        if ((type == QmlFile || type == UrlString)
            && url == QUrl(QStringLiteral("qrc:/qml/QGroundControl/MainWindow/MainWindowSavedState.qml"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/MainWindowSavedState.qml"));
        }
        if ((type == QmlFile || type == UrlString)
            && url == QUrl(QStringLiteral("qrc:/qml/QGroundControl/Toolbar/FlightModeIndicator.qml"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/FlightModeIndicator.qml"));
        }
        if (type == UrlString && url == QUrl(QStringLiteral("qrc:/res/NoVideoBackground.jpg"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/fatum.jpg"));
        }
        if (type == UrlString && url == QUrl(QStringLiteral("qrc:/res/qgroundcontrol.ico"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/herelink-gcs.ico"));
        }
        if ((type == QmlFile || type == UrlString)
            && url == QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlyView/FlyViewToolStripActionList.qml"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/FlyViewToolStripActionList.qml"));
        }
        if ((type == QmlFile || type == UrlString)
            && url == QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/FlyViewCustomLayer.qml"));
        }
        if ((type == QmlFile || type == UrlString)
            && url == QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlyView/FlyViewTopRightColumnLayout.qml"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/FlyViewTopRightColumnLayout.qml"));
        }
        if ((type == QmlFile || type == UrlString)
            && url == QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlyView/FlyViewBottomRightRowLayout.qml"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/FlyViewBottomRightRowLayout.qml"));
        }
        return url;
    }
};
}

Q_APPLICATION_STATIC(HerelinkCorePlugin, herelinkCorePluginInstance);

HerelinkCorePlugin::HerelinkCorePlugin(QObject *parent)
    : QGCCorePlugin(parent)
    , _osd(new HerelinkOsd(this))
{
}

QGCCorePlugin *HerelinkCorePlugin::instance()
{
    return herelinkCorePluginInstance();
}

QQmlApplicationEngine *HerelinkCorePlugin::createQmlApplicationEngine(QObject *parent)
{
    QQmlApplicationEngine *const engine = QGCCorePlugin::createQmlApplicationEngine(parent);
    _osd->start();
    engine->rootContext()->setContextProperty(QStringLiteral("herelinkOsd"), _osd);
    auto *const interceptor = new HerelinkResourceInterceptor;
    engine->addUrlInterceptor(interceptor);
    _interceptors.insert(engine, interceptor);
    connect(engine, &QObject::destroyed, this, [this, engine] {
        delete _interceptors.take(engine);
    });
    return engine;
}

void HerelinkCorePlugin::destroyQmlApplicationEngine(QQmlApplicationEngine *engine)
{
    if (auto *const interceptor = _interceptors.take(engine)) {
        engine->removeUrlInterceptor(interceptor);
        delete interceptor;
    }
    QGCCorePlugin::destroyQmlApplicationEngine(engine);
}

QString HerelinkCorePlugin::stableVersionCheckFileUrl() const
{
    // Preserve the version-check behavior of the previous branding-only build.
    return QStringLiteral("https://s3-us-west-2.amazonaws.com/qgroundcontrol/latest/QGC.version.txt");
}

bool HerelinkCorePlugin::handleJoystickMavlinkAction(Vehicle *vehicle, const QString &label)
{
    if (label == QStringLiteral("Camera Relay OFF")) {
        return _osd->handleCameraSelection(vehicle, 0);
    }
    if (label == QStringLiteral("Camera Relay ON")) {
        return _osd->handleCameraSelection(vehicle, 1);
    }
    return false;
}
