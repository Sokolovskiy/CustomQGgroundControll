#pragma once

#include "QGCCorePlugin.h"
#include <QtCore/QHash>

class QQmlAbstractUrlInterceptor;
class HerelinkOsd;

class HerelinkCorePlugin : public QGCCorePlugin
{
    Q_OBJECT

public:
    explicit HerelinkCorePlugin(QObject *parent = nullptr);
    static QGCCorePlugin *instance();

    QQmlApplicationEngine *createQmlApplicationEngine(QObject *parent) override;
    void destroyQmlApplicationEngine(QQmlApplicationEngine *engine) override;
    QString stableVersionCheckFileUrl() const override;
    bool handleJoystickMavlinkAction(Vehicle *vehicle, const QString &label) override;

private:
    QHash<QQmlApplicationEngine *, QQmlAbstractUrlInterceptor *> _interceptors;
    HerelinkOsd *_osd = nullptr;
};
