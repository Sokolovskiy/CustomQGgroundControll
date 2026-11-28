#include "HerelinkCorePlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtQml/QQmlAbstractUrlInterceptor>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

#include "HerelinkOsd.h"
#include "Joystick.h"
#include "JoystickSettings.h"
#include "QGCLoggingCategory.h"

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
QGC_LOGGING_CATEGORY(HerelinkJoystickLog, "Herelink.Joystick")

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

void HerelinkCorePlugin::initializeJoystickSettings(Joystick* joystick)
{
    // This is the USB layout recorded in the working Windows profile, not an RC/PWM profile.
    static const QRegularExpression deviceName(QStringLiteral("^OpenTX TX16SMK3 Joystick(?: #[0-9]+)?$"));
    if (!joystick || !deviceName.match(joystick->name()).hasMatch() || joystick->axisCount() != 5 ||
        joystick->buttonCount() != 7) {
        return;
    }

    QSettings settings;
    settings.beginGroup(joystick->settings()->settingsGroup());
    const QString migrationKey = QStringLiteral("HerelinkButtonProfileV1");
    if (settings.value(migrationKey, false).toBool()) {
        return;
    }

    // Preserve any existing assignment. After migration, intentional No Action edits stay cleared.
    bool emptyProfile = true;
    for (int button = 2; button <= 6; ++button) {
        emptyProfile = emptyProfile && joystick->getButtonAction(button) == QStringLiteral("No Action");
    }
    if (emptyProfile) {
        const QStringList actions = {QStringLiteral("Altitude Hold"), QStringLiteral("Position Hold"),
                                     QStringLiteral("Land"), QStringLiteral("Camera Relay OFF"),
                                     QStringLiteral("Camera Relay ON")};
        for (int index = 0; index < actions.size(); ++index) {
            joystick->setButtonAction(index + 2, actions[index]);
        }
        qCDebug(HerelinkJoystickLog) << "Restored Windows button profile:" << joystick->name()
                                     << "settings:" << settings.fileName() << settings.group();
    }
    settings.setValue(migrationKey, true);
    settings.sync();
}

QList<QGCCorePlugin::JoystickAction> HerelinkCorePlugin::joystickActions()
{
    return {{QStringLiteral("Camera Relay OFF"), false}, {QStringLiteral("Camera Relay ON"), false}};
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
