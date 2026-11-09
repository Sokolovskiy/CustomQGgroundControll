#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtCore/QTemporaryDir>
#include <QtGui/QQuaternion>

#include "MAVLinkEnums.h"
#include "MAVLinkMessageType.h"

class Vehicle;
class Joystick;
class LinkInterface;

// Presentation model and camera routing for the active vehicle.
class HerelinkOsd : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap data READ data NOTIFY updated)
    Q_PROPERTY(QString callsign READ callsign NOTIFY settingsChanged)
    Q_PROPERTY(QString callsignStatus READ callsignStatus NOTIFY settingsChanged)
    Q_PROPERTY(bool overlayVisible READ overlayVisible WRITE setOverlayVisible NOTIFY settingsChanged)
    Q_PROPERTY(bool attitudeAvailable READ attitudeAvailable NOTIFY updated)
    Q_PROPERTY(QQuaternion attitudeRotation READ attitudeRotation NOTIFY updated)
    Q_PROPERTY(double attitudeRoll READ attitudeRoll NOTIFY updated)
    Q_PROPERTY(double attitudePitch READ attitudePitch NOTIFY updated)
    Q_PROPERTY(QVariantMap attitudeVectors READ attitudeVectors NOTIFY updated)
    Q_PROPERTY(int cellCount READ cellCount WRITE setCellCount NOTIFY settingsChanged)
    Q_PROPERTY(bool vehicleIdentityKnown READ vehicleIdentityKnown NOTIFY settingsChanged)
    Q_PROPERTY(int cameraRelay READ cameraRelay WRITE setCameraRelay NOTIFY settingsChanged)
    Q_PROPERTY(int cam1Value READ cam1Value WRITE setCam1Value NOTIFY settingsChanged)
    Q_PROPERTY(bool herelinkHdmi READ herelinkHdmi WRITE setHerelinkHdmi NOTIFY settingsChanged)
    Q_PROPERTY(bool cameraProfileAvailable READ cameraProfileAvailable NOTIFY settingsChanged)

public:
    explicit HerelinkOsd(QObject *parent = nullptr);
    void start();
    QVariantMap data() const { return _data; }
    QString callsign() const;
    bool overlayVisible() const;
    bool attitudeAvailable() const { return _attitudeAvailable; }
    QQuaternion attitudeRotation() const { return _attitudeRotation; }
    double attitudeRoll() const;
    double attitudePitch() const;
    QVariantMap attitudeVectors() const { return _attitudeVectors; }
    int cellCount() const;
    bool vehicleIdentityKnown() const;
    int cameraRelay() const;
    int cam1Value() const;
    bool herelinkHdmi() const;
    bool cameraProfileAvailable() const;
    void setHerelinkHdmi(bool enabled);
    bool handleCameraSelection(Vehicle *vehicle, int input);
    QString callsignStatus() const { return _callsignStatus; }
    Q_INVOKABLE void reloadCallsign();
    void setOverlayVisible(bool value);
    void setCellCount(int value);
    void setCameraRelay(int value);
    void setCam1Value(int value);

signals:
    void updated();
    void settingsChanged();

private:
    friend class HerelinkOsdTest;
    static qint64 now();
    static QString cellVoltage(double total, int cells);
    static QString decodeCallsign(const QByteArray &bytes);
    void readMetadata(qint64 stamp, bool connected);
    void selectVehicle(Vehicle *vehicle);
    void selectJoystick(Joystick *joystick);
    void receive(const mavlink_message_t &message);
    void receiveHerelink(LinkInterface *link, const mavlink_message_t &message);
    bool sendHerelinkCameraCommand(MAV_CMD command, int input, uint8_t confirmation = 0);
    void resetHerelinkCamera();
    void refresh();
    void observeSent(const QByteArray &bytes);
    void advanceTimer(bool connected, bool armed, qint64 stamp);
    void setAttitudeSample(const QQuaternion &attitude, qint64 stamp);
    void refreshAttitude(bool connected, qint64 stamp);
    void resetAttitude();
    void refreshAttitudeVectors(qint64 stamp);
    QString vehicleKey() const;
    QString cameraProfileKey() const;
    QPointer<Vehicle> _vehicle;
    QPointer<Joystick> _joystick;
    QList<QMetaObject::Connection> _vehicleConnections;
    QMetaObject::Connection _axisConnection;
    QTimer _timer;
    QVariantMap _data;
    QVariantMap _attitudeVectors;
    qint64 _localVelocityAt = -1, _velocityValidityAt = -1;
    bool _horizontalVelocityGood = false, _verticalVelocityGood = false;
    QQuaternion _attitudeSample, _attitudeRotation, _attitudeYawReference;
    qint64 _attitudeAt = -1;
    bool _attitudeSampleValid = false, _attitudeAvailable = false;
    bool _attitudeQuaternionSeen = false, _attitudeYawReferenceKnown = false;
    qint64 _heartbeat = -1, _altitude = -1, _climb = -1, _battery = -1, _axes = -1;
    qint64 _warningAt = -1, _lastTick = -1;
    double _armedSeconds = 0;
    bool _sessionKnown = false, _wasArmed = false;
    bool _altitudeValid = false, _climbValid = false;
    QString _warning;
    int _warningSeverity = 7;
    int _systemStatus = 0;
    float _roll = 0, _pitch = 0, _yaw = 0, _throttle = 0;
    qint64 _cameraTiltAt = -1;
    double _cameraTiltDegrees = 0;
    quint64 _joystickGeneration = 0;
    QPointer<LinkInterface> _link;
    qint64 _radioStatusAt = -1;
    int _radioRssi = 0;
    int _radioNoise = 0;
    QMetaObject::Connection _sentConnection;
    mavlink_message_t _parseMessage{};
    mavlink_status_t _parseStatus{};
    QString _camera = QStringLiteral("CAM —");
    qint64 _cameraHeartbeat = -1;
    qint64 _startupCameraDeadline = 0;
    int _startupCameraInput = -1;
    qint64 _nextHdmiQuery = 0;
    qint64 _hdmiDeadline = 0;
    int _hdmiInput = -1;
    int _hdmiRequestedInput = -1;
    int _hdmiCommandInput = -1;
    bool _hdmiRequestQueued = false;
    bool _hdmiRetried = false;
    qint64 _relayStatus = -1, _relayRequest = -1, _metadataStart = -1, _ftpAttempt = -1;
    qint64 _ftpWaitStart = -1;
    int _relayValue = -1;
    bool _relayRequested = false, _relaySupported = false, _callsignAttempted = false;
    QString _remoteCallsign, _callsignStatus = QStringLiteral("SD callsign not loaded");
    QTemporaryDir _callsignDirectory;
    QString _callsignPath;
    quint64 _callsignSequence = 0;
};
