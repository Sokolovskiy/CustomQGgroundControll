#include "HerelinkOsd.h"

#include <chrono>
#include <cmath>
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QtMath>
#include <QtGui/QVector3D>
#include "FTPManager.h"

#include "BatteryFactGroupListModel.h"
#include "Fact.h"
#include "HealthAndArmingCheckReport.h"
#include "Joystick.h"
#include "JoystickManager.h"
#include "JoystickSettings.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "VehicleLocalPositionFactGroup.h"
#include "VehicleSupports.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(HerelinkControlLog, "Herelink.ControlDiagnostics")

namespace {
constexpr uint8_t kHerelinkAirUnitSystemId = 42;
constexpr qint64 kCameraHeartbeatTimeoutMs = 3000;
constexpr qint64 kStartupCameraSelectionTimeoutMs = 3000;
constexpr qint64 kHdmiQueryIntervalMs = 5000;
constexpr qint64 kHdmiAckTimeoutMs = 900;
constexpr qint64 kHdmiRetryGraceMs = 200;
// Motor centers with the aircraft scale from DroneAttitude3D.qml applied.
constexpr float kAttitudeMotorOffset = 58.0f * 1.13f;
constexpr float kAttitudeMotorHeight = 6.0f * 1.08f;

QString number(double value, int decimals)
{
    return std::isfinite(value) ? QString::number(value, 'f', decimals) : QStringLiteral("—");
}
}

qint64 HerelinkOsd::now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

QString HerelinkOsd::cellVoltage(double total, int cells)
{
    return cells > 0 && cells <= 24 && total > 0 ? number(total / cells, 2) : QStringLiteral("—");
}

HerelinkOsd::HerelinkOsd(QObject *parent) : QObject(parent)
{
    _timer.setInterval(100);
    connect(&_timer, &QTimer::timeout, this, &HerelinkOsd::refresh);
    refresh();
}

void HerelinkOsd::start()
{
    if (_timer.isActive()) {
        return;
    }
    auto *const vehicles = MultiVehicleManager::instance();
    auto *const joysticks = JoystickManager::instance();
    connect(vehicles, &MultiVehicleManager::activeVehicleChanged, this, &HerelinkOsd::selectVehicle);
    connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::messageReceived, this, &HerelinkOsd::receiveHerelink);
    connect(joysticks, &JoystickManager::activeJoystickChanged, this, &HerelinkOsd::selectJoystick);
    connect(joysticks, &JoystickManager::activeJoystickEnabledForActiveVehicleChanged, this,
            [this] { selectJoystick(_joystick); });
    selectVehicle(vehicles->activeVehicle());
    selectJoystick(joysticks->activeJoystick());
    _timer.start();
}

QString HerelinkOsd::callsign() const { return _remoteCallsign; }
bool HerelinkOsd::overlayVisible() const { return QSettings().value(QStringLiteral("HerelinkOSD/visible"), true).toBool(); }
double HerelinkOsd::attitudeRoll() const
{
    return _vehicle ? _vehicle->roll()->rawValue().toDouble() : qQNaN();
}
double HerelinkOsd::attitudePitch() const
{
    return _vehicle ? _vehicle->pitch()->rawValue().toDouble() : qQNaN();
}
QString HerelinkOsd::vehicleKey() const
{
    return vehicleIdentityKnown() ? QStringLiteral("HerelinkOSD/vehicles/%1/").arg(_vehicle->vehicleUIDStr()) : QString();
}
bool HerelinkOsd::vehicleIdentityKnown() const { return _vehicle && _vehicle->vehicleUID() != 0; }

QString HerelinkOsd::cameraProfileKey() const
{
    if (vehicleIdentityKnown()) {
        return vehicleKey() + QStringLiteral("herelinkHdmi");
    }
    if (_vehicle && !_vehicle->vehicleUID2().isEmpty()) {
        // ArduPilot supplies its hardware serial in UID2 and leaves the legacy UID at zero.
        return QStringLiteral("HerelinkOSD/vehicles/uid2-%1/herelinkHdmi")
            .arg(QString::fromLatin1(_vehicle->vehicleUID2().toHex()));
    }
    return {};
}

bool HerelinkOsd::cameraProfileAvailable() const
{
    return !cameraProfileKey().isEmpty();
}

bool HerelinkOsd::herelinkHdmi() const
{
    const QString key = cameraProfileKey();
    return !key.isEmpty() && QSettings().value(key, false).toBool();
}

void HerelinkOsd::setHerelinkHdmi(bool enabled)
{
    const QString key = cameraProfileKey();
    if (!_vehicle || key.isEmpty() || _vehicle->armed()
        || !_data.value(QStringLiteral("connected")).toBool() || herelinkHdmi() == enabled) {
        return;
    }
    QSettings().setValue(key, enabled);
    resetHerelinkCamera();
    _camera = QStringLiteral("CAM —");
    _relayStatus = -1;
    _relayRequested = false;
    emit settingsChanged();
    refresh();
}

bool HerelinkOsd::handleCameraSelection(Vehicle *vehicle, int input)
{
    if (!vehicle || _vehicle != vehicle) {
        return true;
    }
    const bool profileAvailable = cameraProfileAvailable();
    if (profileAvailable && !herelinkHdmi()) {
        return false;
    }
    const qint64 stamp = now();
    const bool cameraReady = _cameraHeartbeat >= 0 && stamp - _cameraHeartbeat <= kCameraHeartbeatTimeoutMs
        && _data.value(QStringLiteral("connected")).toBool();
    if ((!profileAvailable || !cameraReady) && !vehicle->isInitialConnectComplete()) {
        // Joystick polling reports the switch position before vehicle and camera discovery finish.
        if (!vehicle->armed()) {
            _startupCameraInput = input;
            _startupCameraDeadline = stamp + kStartupCameraSelectionTimeoutMs;
        }
        return true;
    }
    if (!profileAvailable) {
        return true;
    }
    if (!cameraReady) {
        qgcApp()->showAppMessage(tr("Herelink camera is unavailable on the active connection."));
        return true;
    }

    _startupCameraInput = -1;
    _hdmiRequestedInput = input;
    // COMMAND_ACK has no stream ID. Keep one switch in flight and coalesce newer input.
    _hdmiRequestQueued = input != _hdmiCommandInput;
    refresh();
    return true;
}

bool HerelinkOsd::sendHerelinkCameraCommand(MAV_CMD command, int input, uint8_t confirmation)
{
    const qint64 stamp = now();
    if (!_vehicle || _heartbeat < 0 || stamp - _heartbeat > 3000
        || _cameraHeartbeat < 0 || stamp - _cameraHeartbeat > kCameraHeartbeatTimeoutMs
        || _vehicle->vehicleLinkManager()->communicationLost()) {
        return false;
    }
    const auto link = _vehicle->vehicleLinkManager()->primaryLink().lock();
    if (!link || _link != link.get() || !link->mavlinkChannelIsSet()) {
        return false;
    }

    // CubePilot selects HDMI 1/2 using stream IDs 0/1 on the camera component.
    const auto *const protocol = MAVLinkProtocol::instance();
    mavlink_message_t message{};
    mavlink_msg_command_long_pack_chan(protocol->getSystemId(), protocol->getComponentId(),
        link->mavlinkChannel(), &message, kHerelinkAirUnitSystemId, MAV_COMP_ID_CAMERA, command,
        confirmation, static_cast<float>(input), 0, 0, 0, 0, 0, 0);
    return _vehicle->sendMessageOnLinkThreadSafe(link.get(), message);
}

void HerelinkOsd::receiveHerelink(LinkInterface *link, const mavlink_message_t &message)
{
    if (message.sysid != kHerelinkAirUnitSystemId || !_vehicle || !link || _link != link
        || _vehicle->vehicleLinkManager()->primaryLink().lock().get() != link) {
        return;
    }
    const qint64 stamp = now();
    if (message.compid == MAV_COMP_ID_TELEMETRY_RADIO && message.msgid == MAVLINK_MSG_ID_RADIO_STATUS) {
        mavlink_radio_status_t radio{};
        mavlink_msg_radio_status_decode(&message, &radio);
        // This Herelink sender reports RSSI and the noise floor as positive dBm magnitudes.
        _radioStatusAt = radio.rssi > 0 && radio.rssi < 128 ? stamp : -1;
        _radioRssi = -static_cast<int>(radio.rssi);
        _radioNoise = radio.noise < 128 ? -static_cast<int>(radio.noise) : 0;
        return;
    }
    if (message.compid != MAV_COMP_ID_CAMERA) {
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
        if (_cameraHeartbeat >= 0 && stamp - _cameraHeartbeat > kCameraHeartbeatTimeoutMs) {
            resetHerelinkCamera();
        }
        _cameraHeartbeat = stamp;
        return;
    }
    if (!herelinkHdmi()) {
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
        mavlink_command_ack_t ack{};
        mavlink_msg_command_ack_decode(&message, &ack);
        const auto *const protocol = MAVLinkProtocol::instance();
        if (ack.command == MAV_CMD_VIDEO_START_STREAMING
            && ack.target_system == protocol->getSystemId()
            && ack.target_component == protocol->getComponentId()) {
            qCInfo(HerelinkControlLog) << "Herelink HDMI command ACK result:" << ack.result
                                       << "pending input:" << _hdmiCommandInput + 1;
            if (_hdmiCommandInput >= 0 && stamp <= _hdmiDeadline) {
                if (ack.result == MAV_RESULT_ACCEPTED) {
                    _hdmiInput = _hdmiCommandInput;
                }
                // Hold a retried command until its deadline so a duplicate ACK cannot complete a new switch.
                if (!_hdmiRetried) {
                    _hdmiCommandInput = -1;
                }
            }
        }
    } else if (message.msgid == MAVLINK_MSG_ID_VIDEO_STREAM_INFORMATION
               && _cameraHeartbeat >= 0 && stamp - _cameraHeartbeat <= kCameraHeartbeatTimeoutMs) {
        mavlink_video_stream_information_t info{};
        mavlink_msg_video_stream_information_decode(&message, &info);
        if (info.stream_id <= 1) {
            _hdmiInput = info.stream_id;
            qCDebug(HerelinkControlLog) << "Herelink reported HDMI input:" << _hdmiInput + 1;
        }
    }
}

void HerelinkOsd::resetHerelinkCamera()
{
    _cameraHeartbeat = -1;
    _startupCameraInput = -1;
    _hdmiInput = -1;
    _hdmiRequestedInput = -1;
    _hdmiCommandInput = -1;
    _hdmiRequestQueued = false;
    _hdmiRetried = false;
    _nextHdmiQuery = 0;
}

int HerelinkOsd::cellCount() const
{
    // This hardware profile uses a user-confirmed 12S battery. No FC parameter is changed.
    const int configured = vehicleIdentityKnown()
        ? QSettings().value(vehicleKey() + QStringLiteral("cells"), 12).toInt() : 12;
    return configured > 0 && configured <= 24 ? configured : 12;
}
int HerelinkOsd::cameraRelay() const { return vehicleIdentityKnown() ? QSettings().value(vehicleKey() + QStringLiteral("relay"), 1).toInt() : 1; }
int HerelinkOsd::cam1Value() const { return vehicleIdentityKnown() ? QSettings().value(vehicleKey() + QStringLiteral("cam1"), 0).toInt() : 0; }
QString HerelinkOsd::decodeCallsign(const QByteArray &bytes)
{
    if (bytes.size() > 4096) { return {}; }
    QString text = QString::fromUtf8(bytes);
    if (text.startsWith(QChar(0xfeff))) { text.remove(0, 1); }
    text = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    for (const QChar character : text) {
        if (!character.isPrint() || character == QChar::ReplacementCharacter) { return {}; }
    }
    return text.left(64);
}

void HerelinkOsd::reloadCallsign()
{
    _remoteCallsign.clear();
    _callsignPath.clear();
    _callsignAttempted = false;
    _metadataStart = -1;
    _ftpAttempt = -1;
    _ftpWaitStart = -1;
    _callsignStatus = QStringLiteral("Waiting for a disarmed, ready connection");
    emit settingsChanged();
}
void HerelinkOsd::setOverlayVisible(bool value)
{
    QSettings().setValue(QStringLiteral("HerelinkOSD/visible"), value);
    emit settingsChanged();
}
void HerelinkOsd::setCellCount(int value)
{
    if (!vehicleIdentityKnown() || value < 0 || value > 24) { return; }
    QSettings().setValue(vehicleKey() + QStringLiteral("cells"), value);
    emit settingsChanged();
    refresh();
}
void HerelinkOsd::setCameraRelay(int value)
{
    if (!vehicleIdentityKnown() || value < -1 || value > 15) { return; }
    QSettings().setValue(vehicleKey() + QStringLiteral("relay"), value);
    _camera = QStringLiteral("CAM —");
    _relayStatus = -1;
    emit settingsChanged();
}
void HerelinkOsd::setCam1Value(int value)
{
    if (!vehicleIdentityKnown() || (value != 0 && value != 1)) { return; }
    QSettings().setValue(vehicleKey() + QStringLiteral("cam1"), value);
    _camera = QStringLiteral("CAM —");
    _relayStatus = -1;
    emit settingsChanged();
}

void HerelinkOsd::selectVehicle(Vehicle *vehicle)
{
    for (const auto &connection : std::as_const(_vehicleConnections)) { disconnect(connection); }
    _vehicleConnections.clear();
    _vehicle = vehicle;
    _radioStatusAt = -1;
    resetAttitude();
    _heartbeat = _altitude = _climb = _battery = -1;
    selectJoystick(_joystick);
    _warningAt = -1;
    _warningSeverity = MAV_SEVERITY_DEBUG;
    _lastTick = -1;
    _sessionKnown = false;
    _wasArmed = false;
    _armedSeconds = 0;
    _camera = QStringLiteral("CAM —");
    _relayStatus = _relayRequest = -1;
    resetHerelinkCamera();
    _relayValue = -1;
    _relayRequested = false;
    _relaySupported = false;
    reloadCallsign();
    _parseMessage = {};
    _parseStatus = {};
    if (vehicle) {
        _vehicleConnections.append(connect(vehicle, &Vehicle::mavlinkMessageReceived, this, &HerelinkOsd::receive));
        _vehicleConnections.append(connect(vehicle, &Vehicle::vehicleUIDChanged, this, &HerelinkOsd::settingsChanged));
        _vehicleConnections.append(connect(vehicle, &Vehicle::armedChanged, this, [vehicle](bool armed) {
            qCInfo(HerelinkControlLog) << "Observed armed state sys/armed:" << vehicle->id() << armed;
        }));
        _vehicleConnections.append(connect(vehicle, &Vehicle::prearmErrorChanged, this,
            [](const QString &error) {
                qCInfo(HerelinkControlLog) << "Pre-arm status:" << error;
            }));
        _vehicleConnections.append(connect(vehicle, &Vehicle::mavCommandResult, this,
            [](int vehicleId, int component, int command, int result, int failure) {
                if (command == MAV_CMD_DO_SET_RELAY || command == MAV_CMD_DO_SET_MODE
                    || command == MAV_CMD_COMPONENT_ARM_DISARM) {
                    qCInfo(HerelinkControlLog) << "Command result sys/comp/command/result/failure:"
                                             << vehicleId << component << command << result << failure;
                }
            }));
        _vehicleConnections.append(connect(vehicle->ftpManager(), &FTPManager::downloadComplete, this,
            [this, vehicle](const QString &path, const QString &error) {
                if (_vehicle != vehicle || path != _callsignPath || path.isEmpty()) { return; }
                QFile file(path);
                if (error.isEmpty() && file.open(QIODevice::ReadOnly)) {
                    _remoteCallsign = decodeCallsign(file.read(4097));
                    _callsignStatus = _remoteCallsign.isEmpty() ? QStringLiteral("callsign.txt is empty or invalid") : QStringLiteral("Read from FC SD: /callsign.txt");
                } else {
                    _callsignStatus = QStringLiteral("Cannot read SD /callsign.txt: %1").arg(error);
                }
                _callsignPath.clear();
                emit settingsChanged();
            }));
    }
    emit settingsChanged();
    refresh();
}

void HerelinkOsd::readMetadata(qint64 stamp, bool connected)
{
    if (!_timer.isActive()) { return; } // Standalone model tests never initiate IO.
    if (!connected) {
        if (_metadataStart >= 0 || !_remoteCallsign.isEmpty()) { reloadCallsign(); }
        _relayRequested = false;
        _relaySupported = false;
        _relayStatus = -1;
        return;
    }
    if (!_vehicle || !_vehicle->isInitialConnectComplete()) { return; }
    if (_metadataStart < 0) { _metadataStart = stamp; }
    if (stamp - _metadataStart < 2000) { return; }
    if (!herelinkHdmi() && (!_relayRequested || (_relaySupported && stamp - _relayRequest >= 5000))) {
        _relayRequested = true;
        _relayRequest = stamp;
        // A read-only message request, never DO_SET_RELAY. The callback owns no
        // context; normal incoming telemetry delivers the response to receive().
        _vehicle->requestMessage([](void *, MAV_RESULT, Vehicle::RequestMessageResultHandlerFailureCode_t,
                                   const mavlink_message_t &) {}, nullptr,
                                _vehicle->defaultComponentId(), MAVLINK_MSG_ID_RELAY_STATUS);
    }
    if (_vehicle->armed()) { return; } // Defer SD traffic until disarmed.
    if (_ftpWaitStart < 0) { _ftpWaitStart = stamp; }
    if (_callsignAttempted || (_ftpAttempt >= 0 && stamp - _ftpAttempt < 1500)) { return; }
    if (stamp - _ftpWaitStart > 30000) {
        _callsignAttempted = true;
        _callsignStatus = QStringLiteral("FTP busy; use Reload after other file operations finish");
        emit settingsChanged();
        return;
    }
    _ftpAttempt = stamp;
    if (!_callsignDirectory.isValid()) { return; }
    const QString filename = QStringLiteral("callsign-%1.txt").arg(++_callsignSequence);
    _callsignPath = QDir(_callsignDirectory.path()).absoluteFilePath(filename);
    // Use the shared manager: download() refuses while another FTP operation is
    // busy. Never cancel another client's operation or reset its sessions.
    if (_vehicle->ftpManager()->download(static_cast<uint8_t>(_vehicle->defaultComponentId()),
                                        QStringLiteral("/callsign.txt"), _callsignDirectory.path(), filename)) {
        _callsignAttempted = true;
        if (!_callsignPath.isEmpty()) { _callsignStatus = QStringLiteral("Reading FC SD /callsign.txt…"); }
        emit settingsChanged();
    } else {
        _callsignPath.clear();
    }
}

void HerelinkOsd::observeSent(const QByteArray &bytes)
{
    // A private parser observes transport output; it does not share a MAVLink
    // channel or signing state with QGC and can never transmit anything.
    if (!_vehicle || _heartbeat < 0 || now() - _heartbeat > 3000
        || _vehicle->vehicleLinkManager()->communicationLost()) { return; }
    for (const char byte : bytes) {
        mavlink_message_t message{};
        mavlink_status_t status{};
        if (mavlink_frame_char_buffer(&_parseMessage, &_parseStatus, static_cast<uint8_t>(byte), &message, &status) != MAVLINK_FRAMING_OK) { continue; }
        if (message.msgid == MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE) {
            mavlink_rc_channels_override_t channels{};
            mavlink_msg_rc_channels_override_decode(&message, &channels);
            if (channels.target_system == _vehicle->id()
                && (channels.target_component == _vehicle->defaultComponentId() || channels.target_component == MAV_COMP_ID_ALL)) {
                // The existing TX16S camera axis uses Additional Axis 6 -> RC10.
                // Observe the calibrated command, never drive or remap the servo.
                if (channels.chan10_raw >= 1000 && channels.chan10_raw <= 2000) {
                    _cameraTiltDegrees = (2000 - channels.chan10_raw) * 0.09;
                    _cameraTiltAt = now();
                } else if (channels.chan10_raw == UINT16_MAX - 1) {
                    _cameraTiltAt = -1;
                }
            }
            continue;
        }
        if (message.msgid != MAVLINK_MSG_ID_COMMAND_LONG) { continue; }
        mavlink_command_long_t command{};
        mavlink_msg_command_long_decode(&message, &command);
        if (command.target_system == _vehicle->id()
            && (command.command == MAV_CMD_DO_SET_RELAY || command.command == MAV_CMD_DO_SET_MODE
                || command.command == MAV_CMD_COMPONENT_ARM_DISARM)) {
            qCInfo(HerelinkControlLog) << "Outgoing sys/comp/command/confirmation/param1/param2:"
                << command.target_system << command.target_component << command.command
                << command.confirmation << command.param1 << command.param2;
        }
        if (command.target_system == _vehicle->id()
            && (command.target_component == _vehicle->defaultComponentId() || command.target_component == MAV_COMP_ID_ALL)
            && cameraRelay() >= 0 && command.command == MAV_CMD_DO_SET_RELAY && command.param1 == cameraRelay()) {
            _camera = (command.param2 == 0 || command.param2 == 1)
                ? (command.param2 == cam1Value() ? QStringLiteral("CAM1 · CMD") : QStringLiteral("CAM2 · CMD"))
                : QStringLiteral("CAM —");
            _relayStatus = -1;
        }
    }
}

void HerelinkOsd::selectJoystick(Joystick *joystick)
{
    disconnect(_axisConnection);
    _joystick = joystick;
    _startupCameraInput = -1;
    _axes = -1;
    _cameraTiltAt = -1;
    const quint64 generation = ++_joystickGeneration;
    if (joystick) {
        // Timestamp on the emitting thread, then copy to the GUI thread. A delayed
        // queued sample must not be mistaken for a fresh physical input sample.
        _axisConnection = connect(joystick, &Joystick::axisValues, this,
            [this, generation](float roll, float pitch, float yaw, float throttle) {
                const qint64 stamp = now();
                QMetaObject::invokeMethod(this, [this, generation, stamp, roll, pitch, yaw, throttle] {
                    if (generation != _joystickGeneration) { return; }
                    _axes = stamp;
                    _roll = roll; _pitch = pitch; _yaw = yaw; _throttle = throttle;
                }, Qt::QueuedConnection);
            }, Qt::DirectConnection);
    }
}

void HerelinkOsd::receive(const mavlink_message_t &message)
{
    if (!_vehicle || message.sysid != _vehicle->id() || message.compid != _vehicle->defaultComponentId()) { return; }
    const qint64 stamp = now();
    switch (message.msgid) {
    case MAVLINK_MSG_ID_LOCAL_POSITION_NED:
        // Velocity values are read from the existing Vehicle Facts after processing.
        _localVelocityAt = stamp;
        break;
    case MAVLINK_MSG_ID_ESTIMATOR_STATUS: {
        mavlink_estimator_status_t status{};
        mavlink_msg_estimator_status_decode(&message, &status);
        _velocityValidityAt = stamp;
        _horizontalVelocityGood = (status.flags & ESTIMATOR_ATTITUDE)
            && (status.flags & ESTIMATOR_VELOCITY_HORIZ) && !(status.flags & ESTIMATOR_CONST_POS_MODE);
        _verticalVelocityGood = (status.flags & ESTIMATOR_ATTITUDE) && (status.flags & ESTIMATOR_VELOCITY_VERT);
        break;
    }
    case MAVLINK_MSG_ID_EKF_STATUS_REPORT: {
        mavlink_ekf_status_report_t status{};
        mavlink_msg_ekf_status_report_decode(&message, &status);
        _velocityValidityAt = stamp;
        _horizontalVelocityGood = (status.flags & EKF_ATTITUDE)
            && (status.flags & EKF_VELOCITY_HORIZ) && !(status.flags & EKF_CONST_POS_MODE);
        _verticalVelocityGood = (status.flags & EKF_ATTITUDE) && (status.flags & EKF_VELOCITY_VERT);
        break;
    }
    case MAVLINK_MSG_ID_ATTITUDE: {
        // Use the existing active Vehicle signal, never a second link/parser.
        // As in VehicleFactGroup, a quaternion stream takes precedence.
        if (_attitudeQuaternionSeen) { break; }
        mavlink_attitude_t attitude{};
        mavlink_msg_attitude_decode(&message, &attitude);
        if (!std::isfinite(attitude.roll) || !std::isfinite(attitude.pitch) || !std::isfinite(attitude.yaw)) {
            setAttitudeSample(QQuaternion(0, 0, 0, 0), stamp);
            break;
        }
        // MAVLink is radians, FRD body -> NED reference, intrinsic Z-Y-X.
        const QQuaternion yaw = QQuaternion::fromAxisAndAngle(0, 0, 1, qRadiansToDegrees(attitude.yaw));
        const QQuaternion pitch = QQuaternion::fromAxisAndAngle(0, 1, 0, qRadiansToDegrees(attitude.pitch));
        const QQuaternion roll = QQuaternion::fromAxisAndAngle(1, 0, 0, qRadiansToDegrees(attitude.roll));
        setAttitudeSample(yaw * pitch * roll, stamp);
        break;
    }
    case MAVLINK_MSG_ID_ATTITUDE_QUATERNION: {
        mavlink_attitude_quaternion_t attitude{};
        mavlink_msg_attitude_quaternion_decode(&message, &attitude);
        QQuaternion rotation(attitude.q1, attitude.q2, attitude.q3, attitude.q4);
        const QQuaternion offset(attitude.repr_offset_q[0], attitude.repr_offset_q[1],
                                 attitude.repr_offset_q[2], attitude.repr_offset_q[3]);
        // Match QGC's representation-offset convention (e.g. tailsitters).
        if (!std::isfinite(offset.lengthSquared())) {
            setAttitudeSample(QQuaternion(0, 0, 0, 0), stamp);
            break;
        }
        if (offset.lengthSquared() >= 0.25f) { rotation *= offset.normalized(); }
        setAttitudeSample(rotation, stamp);
        if (_attitudeSampleValid && !_attitudeQuaternionSeen) {
            _attitudeYawReferenceKnown = false;
            _attitudeQuaternionSeen = true;
        }
        break;
    }
    case MAVLINK_MSG_ID_COMMAND_ACK: {
        mavlink_command_ack_t ack{};
        mavlink_msg_command_ack_decode(&message, &ack);
        if (ack.command == MAV_CMD_DO_SET_RELAY || ack.command == MAV_CMD_DO_SET_MODE
            || ack.command == MAV_CMD_COMPONENT_ARM_DISARM) {
            qCInfo(HerelinkControlLog) << "Incoming ACK source sys/comp/command/result/target sys/comp:"
                << message.sysid << message.compid << ack.command << ack.result
                << ack.target_system << ack.target_component;
        }
        break;
    }
    case MAVLINK_MSG_ID_HEARTBEAT: {
        mavlink_heartbeat_t heartbeat{};
        mavlink_msg_heartbeat_decode(&message, &heartbeat);
        _heartbeat = stamp;
        _systemStatus = heartbeat.system_status;
        break;
    }
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
        mavlink_global_position_int_t position{};
        mavlink_msg_global_position_int_decode(&message, &position);
        // QGC gives ALTITUDE precedence once received. A GLOBAL_POSITION_INT
        // must not refresh an older, different ALTITUDE-backed Fact value.
        if (_vehicle->altitudeRelative()->rawValue().toDouble() == position.relative_alt / 1000.0) {
            _altitude = stamp;
            _altitudeValid = true;
        }
        break;
    }
    case MAVLINK_MSG_ID_ALTITUDE: {
        mavlink_altitude_t altitude{};
        mavlink_msg_altitude_decode(&message, &altitude);
        _altitude = stamp;
        _altitudeValid = std::isfinite(altitude.altitude_relative);
        break;
    }
    case MAVLINK_MSG_ID_VFR_HUD: {
        mavlink_vfr_hud_t hud{};
        mavlink_msg_vfr_hud_decode(&message, &hud);
        _climb = stamp;
        _climbValid = std::isfinite(hud.climb);
        break;
    }
    case MAVLINK_MSG_ID_BATTERY_STATUS: {
        mavlink_battery_status_t battery{};
        mavlink_msg_battery_status_decode(&message, &battery);
        if (battery.id == 0) { _battery = stamp; }
        break;
    }
    case MAVLINK_MSG_ID_RELAY_STATUS: {
        mavlink_relay_status_t relay{};
        mavlink_msg_relay_status_decode(&message, &relay);
        const int index = cameraRelay();
        if (index >= 0 && index < 16 && (relay.present & (1U << index))) {
            _relayValue = (relay.on & (1U << index)) ? 1 : 0;
            _relayStatus = stamp;
            _relaySupported = true;
            _camera = _relayValue == cam1Value() ? QStringLiteral("CAM1 · FC") : QStringLiteral("CAM2 · FC");
        } else {
            _relayStatus = -1;
            _camera = QStringLiteral("CAM —");
        }
        break;
    }
    case MAVLINK_MSG_ID_STATUSTEXT: {
        mavlink_statustext_t text{};
        mavlink_msg_statustext_decode(&message, &text);
        if (text.severity <= MAV_SEVERITY_WARNING
            && (_warningAt < 0 || stamp - _warningAt >= 12000 || text.severity <= _warningSeverity)) {
            _warning = QString::fromUtf8(text.text, static_cast<int>(strnlen(text.text, sizeof(text.text))));
            _warningAt = stamp;
            _warningSeverity = text.severity;
        }
        break;
    }
    default: break;
    }
}

void HerelinkOsd::advanceTimer(bool connected, bool armed, qint64 stamp)
{
    if (!connected) {
        _sessionKnown = false;
        _armedSeconds = 0;
        _wasArmed = false;
    } else if (armed) {
        if (!_wasArmed) { _armedSeconds = 0; _sessionKnown = true; }
        if (_wasArmed && _lastTick >= 0) { _armedSeconds += static_cast<double>(stamp - _lastTick) / 1000.0; }
    }
    _wasArmed = connected && armed;
    _lastTick = stamp;
}

void HerelinkOsd::resetAttitude()
{
    _localVelocityAt = _velocityValidityAt = -1;
    _horizontalVelocityGood = _verticalVelocityGood = false;
    _attitudeAt = -1;
    _attitudeSampleValid = _attitudeAvailable = false;
    _attitudeQuaternionSeen = _attitudeYawReferenceKnown = false;
    _attitudeSample = _attitudeRotation = _attitudeYawReference = QQuaternion();
}

void HerelinkOsd::setAttitudeSample(const QQuaternion &attitude, qint64 stamp)
{
    const float normSquared = attitude.lengthSquared();
    if (_attitudeAt >= 0 && stamp - _attitudeAt > 1000) { _attitudeYawReferenceKnown = false; }
    _attitudeAt = stamp;
    _attitudeSampleValid = std::isfinite(normSquared) && normSquared >= 0.25f && normSquared <= 2.25f;
    if (_attitudeSampleValid) { _attitudeSample = attitude.normalized(); }
    else { _attitudeYawReferenceKnown = false; }
}

void HerelinkOsd::refreshAttitude(bool connected, qint64 stamp)
{
    _attitudeAvailable = false;
    if (!connected) {
        resetAttitude();
        return;
    }
    if (!_attitudeSampleValid || _attitudeAt < 0 || stamp - _attitudeAt > 1000) {
        _attitudeYawReferenceKnown = false;
        return;
    }
    if (!_attitudeYawReferenceKnown) {
        const QVector3D forward = _attitudeSample.rotatedVector(QVector3D(1, 0, 0));
        // Yaw is undefined with a vertical nose; wait for a usable horizontal projection.
        if (forward.x() * forward.x() + forward.y() * forward.y() < 0.000001f) { return; }
        const float yawDegrees = qRadiansToDegrees(std::atan2(forward.y(), forward.x()));
        _attitudeYawReference = QQuaternion::fromAxisAndAngle(0, 0, 1, yawDegrees);
        _attitudeYawReferenceKnown = true;
    }
    const QQuaternion relative = _attitudeYawReference.conjugated() * _attitudeSample;
    // NED (forward, right, down) -> scene (right, up, back): (N,E,D) -> (E,-D,-N).
    // Apply the same basis change to both body and reference, not Qt's Euler order.
    const QQuaternion basis(0.5f, 0.5f, 0.5f, -0.5f);
    _attitudeRotation = (basis * relative * basis.conjugated()).normalized();
    _attitudeAvailable = true;
}

void HerelinkOsd::refreshAttitudeVectors(qint64 stamp)
{
    QVariantList motorTilts;
    for (int i = 0; i < 4; ++i) {
        motorTilts.append(QVariantMap{
            {QStringLiteral("lower"), false},
            {QStringLiteral("position"), QVariant::fromValue(QVector3D())}
        });
    }
    _attitudeVectors = {
        {QStringLiteral("motorTilts"), motorTilts},
        {QStringLiteral("motionAvailable"), false}, {QStringLiteral("motionVisible"), false},
        {QStringLiteral("motionRotation"), QVariant::fromValue(QQuaternion())},
        {QStringLiteral("motionSpeed"), 0.0}, {QStringLiteral("motionAxes"), QStringLiteral("—")}
    };
    if (!_vehicle || !_attitudeAvailable) { return; }
    const QVector3D up = _attitudeRotation.rotatedVector(QVector3D(0, 1, 0));
    // Ignore total inclination <= 1 degree. Use motor height relative to the
    // aircraft center, not the decorative horizontal outline below the model.
    if (up.y() < std::cos(qDegreesToRadians(1.0f))) {
        for (int i = 0; i < 4; ++i) {
            const float x = i % 2 == 0 ? -kAttitudeMotorOffset : kAttitudeMotorOffset;
            const float z = i < 2 ? -kAttitudeMotorOffset : kAttitudeMotorOffset;
            const QVector3D arm = _attitudeRotation.rotatedVector(QVector3D(x, 0, z));
            const QVector3D motor = _attitudeRotation.rotatedVector(QVector3D(x, kAttitudeMotorHeight, z));
            const QVector3D outside = QVector3D(arm.x(), 0, arm.z()).normalized() * 24.0f;
            motorTilts[i] = QVariantMap{
                {QStringLiteral("lower"), arm.y() < -0.05f},
                {QStringLiteral("position"), QVariant::fromValue(motor + outside + QVector3D(0, 12, 0))}
            };
        }
        _attitudeVectors[QStringLiteral("motorTilts")] = motorTilts;
    }

    const auto fresh = [stamp](qint64 at, qint64 age) { return at >= 0 && stamp - at <= age; };
    if (!fresh(_velocityValidityAt, 3000)) { return; }
    auto *const local = qobject_cast<VehicleLocalPositionFactGroup *>(_vehicle->localPositionFactGroup());
    const bool localFresh = local && fresh(_localVelocityAt, 1500);
    const double vx = localFresh ? local->vx()->rawValue().toDouble() : qQNaN();
    const double vy = localFresh ? local->vy()->rawValue().toDouble() : qQNaN();
    double vz = localFresh ? local->vz()->rawValue().toDouble() : qQNaN();
    if (!std::isfinite(vz) && _climbValid && fresh(_climb, 1500)) {
        vz = -_vehicle->climbRate()->rawValue().toDouble(); // NED down versus climb up.
    }
    const bool horizontal = _horizontalVelocityGood && std::isfinite(vx) && std::isfinite(vy);
    const bool vertical = _verticalVelocityGood && std::isfinite(vz);
    if (!horizontal && !vertical) { return; }
    // Unknown axes are omitted and explicitly identified in the displayed scope.
    const QVector3D nativeVelocity(horizontal ? static_cast<float>(vx) : 0.0f,
                                   horizontal ? static_cast<float>(vy) : 0.0f,
                                   vertical ? static_cast<float>(vz) : 0.0f);
    const QQuaternion basis(0.5f, 0.5f, 0.5f, -0.5f);
    const QVector3D velocity = basis.rotatedVector(
        _attitudeYawReference.conjugated().rotatedVector(nativeVelocity));
    const float speed = velocity.length();
    if (!std::isfinite(speed)) { return; }
    _attitudeVectors[QStringLiteral("motionAvailable")] = true;
    _attitudeVectors[QStringLiteral("motionSpeed")] = speed;
    _attitudeVectors[QStringLiteral("motionAxes")] = horizontal
        ? (vertical ? QStringLiteral("XYZ") : QStringLiteral("XY")) : QStringLiteral("Z");
    // Suppress direction jitter near zero speed; retain the actual numeric value.
    if (speed >= 0.2f) {
        _attitudeVectors[QStringLiteral("motionVisible")] = true;
        _attitudeVectors[QStringLiteral("motionRotation")] = QVariant::fromValue(
            QQuaternion::rotationTo(QVector3D(0, 0, -1), velocity.normalized()));
    }
}

void HerelinkOsd::refresh()
{
    const qint64 stamp = now();
    const auto fresh = [stamp](qint64 value, qint64 age) { return value >= 0 && stamp - value <= age; };
    const bool connected = _vehicle && fresh(_heartbeat, 3000) && !_vehicle->vehicleLinkManager()->communicationLost();
    const bool armed = connected && _vehicle->armed();
    readMetadata(stamp, connected);
    if (_relayStatus >= 0 && !fresh(_relayStatus, 15000)) {
        _camera = QStringLiteral("CAM —");
    }
    LinkInterface *const link = _vehicle ? _vehicle->vehicleLinkManager()->primaryLink().lock().get() : nullptr;
    if (_link != link) {
        disconnect(_sentConnection);
        _link = link;
        _radioStatusAt = -1;
        _cameraTiltAt = -1;
        resetAttitude();
        _camera = QStringLiteral("CAM —");
        resetHerelinkCamera();
        _parseMessage = {};
        _parseStatus = {};
        if (link) {
            _sentConnection = connect(link, &LinkInterface::bytesSent, this,
                [this, link](LinkInterface *, const QByteArray &bytes) {
                    if (_link == link) { observeSent(bytes); }
                });
        }
    }
    refreshAttitude(connected, stamp);
    refreshAttitudeVectors(stamp);
    advanceTimer(connected, armed, stamp);
    if (!connected) {
        _cameraTiltAt = -1;
        _radioStatusAt = -1;
        _camera = QStringLiteral("CAM —");
        resetHerelinkCamera();
        _parseMessage = {};
        _parseStatus = {};
    }
    const bool hdmi = connected && herelinkHdmi();
    const bool hdmiOnline = hdmi && fresh(_cameraHeartbeat, kCameraHeartbeatTimeoutMs);
    if (_startupCameraInput >= 0) {
        if (armed || stamp >= _startupCameraDeadline || (!hdmi && cameraProfileAvailable())) {
            _startupCameraInput = -1;
        } else if (hdmiOnline) {
            _hdmiRequestedInput = _startupCameraInput;
            _hdmiRequestQueued = true;
            _startupCameraInput = -1;
        }
    }
    if (hdmiOnline && _hdmiCommandInput >= 0 && stamp >= _hdmiDeadline) {
        if (!_hdmiRetried && !_hdmiRequestQueued && _hdmiInput != _hdmiCommandInput
            && stamp - _hdmiDeadline <= kHdmiRetryGraceMs
            && sendHerelinkCameraCommand(MAV_CMD_VIDEO_START_STREAMING, _hdmiCommandInput, 1)) {
            _hdmiRetried = true;
            _hdmiDeadline = stamp + kHdmiAckTimeoutMs;
            qCInfo(HerelinkControlLog) << "Retried Herelink HDMI input:" << _hdmiCommandInput + 1;
        } else {
            _hdmiCommandInput = -1;
        }
    }
    if (hdmiOnline && _hdmiCommandInput < 0 && _hdmiRequestQueued) {
        _hdmiRequestQueued = false;
        if (sendHerelinkCameraCommand(MAV_CMD_VIDEO_START_STREAMING, _hdmiRequestedInput)) {
            _hdmiInput = -1;
            _hdmiCommandInput = _hdmiRequestedInput;
            _hdmiRetried = false;
            _hdmiDeadline = stamp + kHdmiAckTimeoutMs;
            _nextHdmiQuery = stamp + 1000;
            qCInfo(HerelinkControlLog) << "Requested Herelink HDMI input:" << _hdmiCommandInput + 1;
        }
    }
    if (hdmiOnline && _hdmiCommandInput < 0 && stamp >= _nextHdmiQuery) {
        _nextHdmiQuery = stamp + kHdmiQueryIntervalMs;
        sendHerelinkCameraCommand(MAV_CMD_REQUEST_VIDEO_STREAM_INFORMATION, 0);
    }
    const QString camera = hdmi
        ? (hdmiOnline && _hdmiInput >= 0
           ? (_hdmiInput == 0 ? QStringLiteral("CAM1") : QStringLiteral("CAM2"))
           : QStringLiteral("CAM —"))
        : _camera;

    QString banner = QStringLiteral("Vehicle disconnected / data stale");
    QString tone = QStringLiteral("muted");
    if (connected) {
        banner = armed ? QStringLiteral("ARMED — use caution") : QString();
        if (armed) { tone = QStringLiteral("warning"); }
        if (!armed && _vehicle->healthAndArmingCheckReport()->supported()
            && _vehicle->healthAndArmingCheckReport()->canArm()
            && !_vehicle->healthAndArmingCheckReport()->hasWarningsOrErrors()
            && _vehicle->isInitialConnectComplete()) {
            banner = QStringLiteral("PreArm checks OK"); tone = QStringLiteral("ok");
        }
        const bool seriousText = fresh(_warningAt, 12000) && _warningSeverity <= MAV_SEVERITY_ERROR;
        if (fresh(_warningAt, 12000)) {
            banner = _warning;
            tone = seriousText ? QStringLiteral("critical") : QStringLiteral("warning");
        }
        if (!_vehicle->prearmError().isEmpty() && !seriousText) {
            banner = _vehicle->prearmError(); tone = QStringLiteral("warning");
        }
        if (_systemStatus == MAV_STATE_CRITICAL || _systemStatus == MAV_STATE_EMERGENCY) {
            banner = QStringLiteral("CRITICAL vehicle state — check QGC warnings"); tone = QStringLiteral("critical");
        }
    }
    QString voltage = QStringLiteral("—");
    double voltagePerCell = qQNaN();
    const QString voltageUnit = QStringLiteral("V/cell");
    double current = qQNaN();
    if (connected && fresh(_battery, 5000)) {
        auto *const batteries = _vehicle->batteries();
        for (int i = 0; i < batteries->count(); ++i) {
            auto *const battery = qobject_cast<BatteryFactGroup *>(batteries->get(i));
            if (battery && battery->id()->rawValue().toInt() == 0) {
                const double total = battery->voltage()->rawValue().toDouble();
                voltage = cellVoltage(total, cellCount());
                if (std::isfinite(total) && total > 0) { voltagePerCell = total / cellCount(); }
                current = battery->current()->rawValue().toDouble();
                break;
            }
        }
    }
    const bool sticks = connected && _joystick && _joystick->isRunning() && fresh(_axes, 300)
        && JoystickManager::instance()->activeJoystickEnabledForActiveVehicle()
        && _joystick->settings()->calibrated()->rawValue().toBool();
    const bool centerZero = sticks && _joystick->settings()->throttleModeCenterZero()->rawValue().toBool()
        && _vehicle->supports()->throttleModeCenterZero();
    const bool cameraTiltAvailable = sticks && fresh(_cameraTiltAt, 1000)
        && _joystick->settings()->additionalAxesFunction()->rawValue().toInt() == 1
        && _joystick->settings()->enableAdditionalAxis6()->rawValue().toBool();
    const int seconds = static_cast<int>(_armedSeconds);
    const bool radioAvailable = connected && fresh(_radioStatusAt, 3000);
    _data = {
        {QStringLiteral("connected"), connected}, {QStringLiteral("armed"), armed},
        {QStringLiteral("mode"), connected ? _vehicle->flightMode() : QStringLiteral("—")},
        {QStringLiteral("banner"), banner}, {QStringLiteral("tone"), tone},
        {QStringLiteral("voltage"), voltage}, {QStringLiteral("current"), number(current, 1)},
        {QStringLiteral("voltageUnit"), voltageUnit},
        {QStringLiteral("voltagePerCell"), voltagePerCell},
        {QStringLiteral("radioRssi"), radioAvailable ? QString::number(_radioRssi) : QStringLiteral("—")},
        {QStringLiteral("radioSnr"), radioAvailable && _radioNoise < 0
            ? QString::number(_radioRssi - _radioNoise) : QStringLiteral("—")},
        {QStringLiteral("motorOutput"), connected && fresh(_climb, 3000) && _vehicle->throttlePct()->rawValue().toInt() >= 0 && _vehicle->throttlePct()->rawValue().toInt() <= 100 ? QString::number(_vehicle->throttlePct()->rawValue().toInt()) : QStringLiteral("—")},
        {QStringLiteral("altitude"), connected && _altitudeValid && fresh(_altitude, 3000) ? number(_vehicle->altitudeRelative()->rawValue().toDouble(), 1) : QStringLiteral("—")},
        {QStringLiteral("climb"), connected && _climbValid && fresh(_climb, 3000) ? number(_vehicle->climbRate()->rawValue().toDouble(), 1) : QStringLiteral("—")},
        {QStringLiteral("timer"), connected && _sessionKnown ? QStringLiteral("%1:%2:%3").arg(seconds / 3600, 2, 10, QLatin1Char('0')).arg(seconds / 60 % 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0')) : QStringLiteral("—:—:—")},
        {QStringLiteral("sticks"), sticks}, {QStringLiteral("roll"), _roll}, {QStringLiteral("pitch"), _pitch},
        {QStringLiteral("yaw"), _yaw}, {QStringLiteral("throttle"), centerZero ? _throttle : _throttle * 2 - 1},
        {QStringLiteral("camera"), camera},
        {QStringLiteral("cameraRequested"), hdmiOnline ? _hdmiRequestedInput + 1 : 0},
        {QStringLiteral("cameraTiltAvailable"), cameraTiltAvailable},
        {QStringLiteral("cameraTiltDegrees"), cameraTiltAvailable ? _cameraTiltDegrees : qQNaN()}
    };
    emit updated();
}
