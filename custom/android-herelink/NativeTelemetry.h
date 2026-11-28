#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtGui/QVector3D>
#include <QtQmlIntegration/QtQmlIntegration>
#include <limits>

#include "QGCMAVLink.h"

class LinkInterface;
class Vehicle;

// Passive observations only: no links, parameter writes or command senders.
class NativeTelemetry : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList channels READ channels NOTIFY updated)
    Q_PROPERTY(bool channelsAvailable READ channelsAvailable NOTIFY updated)
    Q_PROPERTY(QString radioRssi READ radioRssi NOTIFY updated)
    Q_PROPERTY(QString radioSnr READ radioSnr NOTIFY updated)
    Q_PROPERTY(bool attitudeAvailable READ attitudeAvailable NOTIFY updated)
    Q_PROPERTY(QVariantMap motion READ motion NOTIFY updated)
    Q_PROPERTY(double yawReference READ yawReference WRITE setYawReference NOTIFY updated)

public:
    explicit NativeTelemetry(QObject* parent = nullptr);

    QVariantList channels() const { return _channels; }

    bool channelsAvailable() const;
    QString radioRssi() const;
    QString radioSnr() const;
    bool attitudeAvailable() const;
    QVariantMap motion() const;

    double yawReference() const { return _yawReference; }

    void setYawReference(double heading);
    Q_INVOKABLE double axis(const QString& mapping) const;

signals:
    void updated();

private:
    void selectVehicle(Vehicle* vehicle);
    void receive(LinkInterface* link, const mavlink_message_t& message);
    bool fresh(qint64 stamp, qint64 maxAge = 1500) const;
    void resetObservations();
    QPointer<Vehicle> _vehicle;
    QPointer<LinkInterface> _link;
    QTimer _timer;
    QElapsedTimer _clock;
    QVariantList _channels;
    qint64 _channelsAt = -1;
    qint64 _radioAt = -1;
    qint64 _attitudeAt = -1;
    qint64 _velocityAt = -1;
    qint64 _velocityValidityAt = -1;
    bool _horizontalVelocityGood = false;
    bool _verticalVelocityGood = false;
    QVector3D _velocity;
    double _yawReference = std::numeric_limits<double>::quiet_NaN();
    int _rssi = 0;
    int _noise = 0;
};
