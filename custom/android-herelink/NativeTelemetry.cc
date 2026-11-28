#include "NativeTelemetry.h"

#include <QtCore/QtMath>
#include <QtGui/QQuaternion>
#include <cmath>

#include "LinkInterface.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "ParameterManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

NativeTelemetry::NativeTelemetry(QObject* parent) : QObject(parent)
{
    _clock.start();
    auto* manager = MultiVehicleManager::instance();
    connect(manager, &MultiVehicleManager::activeVehicleChanged, this, &NativeTelemetry::selectVehicle);
    connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::messageReceived, this, &NativeTelemetry::receive);
    connect(&_timer, &QTimer::timeout, this, &NativeTelemetry::updated);
    selectVehicle(manager->activeVehicle());
    _timer.start(250);
}

void NativeTelemetry::selectVehicle(Vehicle* vehicle)
{
    _vehicle = vehicle;
    _link = nullptr;
    resetObservations();
    emit updated();
}

void NativeTelemetry::resetObservations()
{
    _channels.clear();
    _channelsAt = _radioAt = _attitudeAt = -1;
    _velocityAt = _velocityValidityAt = -1;
    _horizontalVelocityGood = _verticalVelocityGood = false;
}

bool NativeTelemetry::fresh(qint64 stamp, qint64 maxAge) const
{
    if (!_vehicle || _vehicle->vehicleLinkManager()->communicationLost()) {
        return false;
    }
    auto link = _vehicle->vehicleLinkManager()->primaryLink().lock();
    return link && link.get() == _link && stamp >= 0 && _clock.elapsed() - stamp <= maxAge;
}

bool NativeTelemetry::channelsAvailable() const
{
    return fresh(_channelsAt);
}

bool NativeTelemetry::attitudeAvailable() const
{
    return fresh(_attitudeAt);
}

QString NativeTelemetry::radioRssi() const
{
    return fresh(_radioAt) ? QString::number(_rssi) : QStringLiteral("—");
}

QString NativeTelemetry::radioSnr() const
{
    return fresh(_radioAt) ? QString::number(_rssi - _noise) : QStringLiteral("—");
}

QVariantMap NativeTelemetry::motion() const
{
    QVariantMap result{{QStringLiteral("available"), false},
                       {QStringLiteral("visible"), false},
                       {QStringLiteral("rotation"), QVariant::fromValue(QQuaternion())},
                       {QStringLiteral("speed"), 0.0},
                       {QStringLiteral("axes"), QStringLiteral("—")}};
    if (!_vehicle || !attitudeAvailable() || !fresh(_velocityAt) || !fresh(_velocityValidityAt, 3000)) {
        return result;
    }
    const double heading = _yawReference;
    const bool horizontal = _horizontalVelocityGood && std::isfinite(_velocity.x()) && std::isfinite(_velocity.y()) &&
                            std::isfinite(heading);
    const bool vertical = _verticalVelocityGood && std::isfinite(_velocity.z());
    if (!horizontal && !vertical) {
        return result;
    }
    const double yaw = horizontal ? qDegreesToRadians(heading) : 0;
    const float north = horizontal ? _velocity.x() : 0;
    const float east = horizontal ? _velocity.y() : 0;
    // Use the same initial-heading reference as the aircraft model.
    const QVector3D velocity(-std::sin(yaw) * north + std::cos(yaw) * east, vertical ? -_velocity.z() : 0,
                             -std::cos(yaw) * north - std::sin(yaw) * east);
    const float speed = velocity.length();
    if (!std::isfinite(speed)) {
        return result;
    }
    result[QStringLiteral("available")] = true;
    result[QStringLiteral("visible")] = speed >= 0.2f;
    result[QStringLiteral("speed")] = speed;
    result[QStringLiteral("axes")] =
        horizontal ? (vertical ? QStringLiteral("XYZ") : QStringLiteral("XY")) : QStringLiteral("Z");
    if (speed >= 0.2f) {
        result[QStringLiteral("rotation")] =
            QVariant::fromValue(QQuaternion::rotationTo(QVector3D(0, 0, -1), velocity.normalized()));
    }
    return result;
}

void NativeTelemetry::setYawReference(double heading)
{
    if (_yawReference == heading || (std::isnan(_yawReference) && std::isnan(heading))) {
        return;
    }
    _yawReference = heading;
    emit updated();
}

double NativeTelemetry::axis(const QString& mapping) const
{
    if (!_vehicle || !channelsAvailable()) {
        return qQNaN();
    }
    auto* parameters = _vehicle->parameterManager();
    const auto value = [parameters](const QString& name) {
        if (!parameters || !parameters->parameterExists(-1, name)) {
            return qQNaN();
        }
        const auto* fact = parameters->getParameter(-1, name);
        return fact ? fact->rawValue().toDouble() : qQNaN();
    };
    const double mapped = value(mapping);
    if (!qIsFinite(mapped) || mapped < 1 || mapped > _channels.size() || mapped != static_cast<int>(mapped)) {
        return qQNaN();
    }
    const int channel = static_cast<int>(mapped);
    const QString prefix = QStringLiteral("RC%1_").arg(channel);
    const double minimum = value(prefix + QStringLiteral("MIN"));
    const double maximum = value(prefix + QStringLiteral("MAX"));
    const double trim =
        mapping == QStringLiteral("RCMAP_THROTTLE") ? (minimum + maximum) / 2 : value(prefix + QStringLiteral("TRIM"));
    const double reversed = value(prefix + QStringLiteral("REVERSED"));
    const int pwm = _channels[channel - 1].toInt();
    if (pwm < 0 || !(minimum < trim && trim < maximum) || !qIsFinite(reversed)) {
        return qQNaN();
    }
    const double normalized = pwm >= trim ? (pwm - trim) / (maximum - trim) : (pwm - trim) / (trim - minimum);
    return qBound(-1.0, normalized, 1.0) * (reversed == 1 ? -1 : 1);
}

void NativeTelemetry::receive(LinkInterface* link, const mavlink_message_t& message)
{
    if (!_vehicle || !link || _vehicle->vehicleLinkManager()->primaryLink().lock().get() != link) {
        return;
    }
    if (_link != link) {
        _link = link;
        resetObservations();
    }
    // Herelink's radio service reports positive magnitudes of negative dBm.
    if (message.sysid == 42 && message.compid == MAV_COMP_ID_TELEMETRY_RADIO &&
        message.msgid == MAVLINK_MSG_ID_RADIO_STATUS) {
        mavlink_radio_status_t radio{};
        mavlink_msg_radio_status_decode(&message, &radio);
        _radioAt = radio.rssi > 0 && radio.rssi < 128 && radio.noise > 0 && radio.noise < 128 ? _clock.elapsed() : -1;
        _rssi = -static_cast<int>(radio.rssi);
        _noise = -static_cast<int>(radio.noise);
        emit updated();
    }
    if (message.sysid != _vehicle->id() || message.compid != _vehicle->defaultComponentId()) {
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_ATTITUDE) {
        mavlink_attitude_t attitude{};
        mavlink_msg_attitude_decode(&message, &attitude);
        _attitudeAt = std::isfinite(attitude.roll) && std::isfinite(attitude.pitch) && std::isfinite(attitude.yaw)
                          ? _clock.elapsed()
                          : -1;
        emit updated();
    } else if (message.msgid == MAVLINK_MSG_ID_ATTITUDE_QUATERNION) {
        mavlink_attitude_quaternion_t attitude{};
        mavlink_msg_attitude_quaternion_decode(&message, &attitude);
        const QQuaternion rotation(attitude.q1, attitude.q2, attitude.q3, attitude.q4);
        const float norm = rotation.lengthSquared();
        _attitudeAt = std::isfinite(norm) && norm >= 0.25f && norm <= 2.25f ? _clock.elapsed() : -1;
        emit updated();
    } else if (message.msgid == MAVLINK_MSG_ID_LOCAL_POSITION_NED) {
        mavlink_local_position_ned_t position{};
        mavlink_msg_local_position_ned_decode(&message, &position);
        _velocity = QVector3D(position.vx, position.vy, position.vz);
        _velocityAt = _clock.elapsed();
        emit updated();
    } else if (message.msgid == MAVLINK_MSG_ID_ESTIMATOR_STATUS) {
        mavlink_estimator_status_t status{};
        mavlink_msg_estimator_status_decode(&message, &status);
        _horizontalVelocityGood = (status.flags & ESTIMATOR_ATTITUDE) && (status.flags & ESTIMATOR_VELOCITY_HORIZ) &&
                                  !(status.flags & ESTIMATOR_CONST_POS_MODE);
        _verticalVelocityGood = (status.flags & ESTIMATOR_ATTITUDE) && (status.flags & ESTIMATOR_VELOCITY_VERT);
        _velocityValidityAt = _clock.elapsed();
        emit updated();
    } else if (message.msgid == MAVLINK_MSG_ID_EKF_STATUS_REPORT) {
        mavlink_ekf_status_report_t status{};
        mavlink_msg_ekf_status_report_decode(&message, &status);
        _horizontalVelocityGood = (status.flags & EKF_ATTITUDE) && (status.flags & EKF_VELOCITY_HORIZ) &&
                                  !(status.flags & EKF_CONST_POS_MODE);
        _verticalVelocityGood = (status.flags & EKF_ATTITUDE) && (status.flags & EKF_VELOCITY_VERT);
        _velocityValidityAt = _clock.elapsed();
        emit updated();
    } else if (message.msgid == MAVLINK_MSG_ID_RC_CHANNELS) {
        mavlink_rc_channels_t rc{};
        mavlink_msg_rc_channels_decode(&message, &rc);
        const uint16_t values[] = {rc.chan1_raw,  rc.chan2_raw,  rc.chan3_raw,  rc.chan4_raw,  rc.chan5_raw,
                                   rc.chan6_raw,  rc.chan7_raw,  rc.chan8_raw,  rc.chan9_raw,  rc.chan10_raw,
                                   rc.chan11_raw, rc.chan12_raw, rc.chan13_raw, rc.chan14_raw, rc.chan15_raw,
                                   rc.chan16_raw, rc.chan17_raw, rc.chan18_raw};
        _channels.clear();
        for (unsigned i = 0; i < qMin<unsigned>(rc.chancount, 18); ++i) {
            _channels.append(values[i] >= 800 && values[i] <= 2200 ? static_cast<int>(values[i]) : -1);
        }
        _channelsAt = _clock.elapsed();
        emit updated();
    }
}
