#pragma once

#include <QtCore/QObject>
#include <QtQml/QQmlAbstractUrlInterceptor>
#include <QtQml/QQmlApplicationEngine>

// Only presentation resources are intercepted; the CubePilot plugin owns controls.
class DisplayResources final : public QObject, public QQmlAbstractUrlInterceptor
{
public:
    explicit DisplayResources(QObject* parent) : QObject(parent) {}

    QUrl intercept(const QUrl& url, DataType type) override
    {
        if ((type == QmlFile || type == UrlString) &&
            url == QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlightDisplay/FlyViewCustomLayer.qml"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/FlyViewCustomLayer.qml"));
        }
        if (type == UrlString && url == QUrl(QStringLiteral("qrc:/res/NoVideoBackground.jpg"))) {
            return QUrl(QStringLiteral("qrc:/Herelink/fatum.jpg"));
        }
        return url;
    }
};
