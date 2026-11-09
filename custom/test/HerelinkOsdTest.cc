#include "HerelinkOsdTest.h"
#include "HerelinkOsd.h"
#include "Vehicle.h"
#include "Fact.h"
#include "MockJoystick.h"
#include "BatteryFactGroupListModel.h"
#include "FTPManager.h"
#include <QtCore/QFile>

UT_REGISTER_TEST(HerelinkOsdTest, TestLabel::Unit)

void HerelinkOsdTest::disconnectedAndSettings()
{
    HerelinkOsd osd;
    QCOMPARE(osd.data().value("connected").toBool(), false);
    for (const auto *key : {"altitude", "climb", "voltage", "current"}) {
        QCOMPARE(osd.data().value(QLatin1String(key)).toString(), QStringLiteral("—"));
    }
    QCOMPARE(osd.data().value("camera").toString(), QStringLiteral("CAM —"));
    QVERIFY(!osd.data().value("sticks").toBool());
    QVERIFY(osd.data().value("banner").toString() != QStringLiteral("PreArm checks OK"));
    QVERIFY(!osd.vehicleIdentityKnown());
    osd.setCellCount(6);
    QCOMPARE(osd.cellCount(), 0);
    osd.setOverlayVisible(false);
    HerelinkOsd reloaded;
    QVERIFY(reloaded.callsign().isEmpty());
    QCOMPARE(HerelinkOsd::decodeCallsign(QByteArray("\xef\xbb\xbf" "DUAL3\r\n")), QStringLiteral("DUAL3"));
    QCOMPARE(HerelinkOsd::decodeCallsign(QByteArray(90, 'X')).size(), 64);
    QVERIFY(HerelinkOsd::decodeCallsign(QByteArray("a\0b", 3)).isEmpty());
    QVERIFY(!reloaded.overlayVisible());
    osd.setOverlayVisible(true);

    Vehicle vehicle(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_QUADROTOR);
    osd.selectVehicle(&vehicle);
    osd._callsignPath = osd._callsignDirectory.filePath(QStringLiteral("fixture.txt"));
    QFile fixture(osd._callsignPath);
    QVERIFY(fixture.open(QIODevice::WriteOnly));
    QCOMPARE(fixture.write("DUAL3\r\n"), qint64(7));
    fixture.close();
    // Simulate the shared FTP completion. No network request is made.
    emit vehicle.ftpManager()->downloadComplete(QStringLiteral("unrelated-file"), QString());
    QVERIFY(osd.callsign().isEmpty());
    emit vehicle.ftpManager()->downloadComplete(osd._callsignPath, QString());
    QCOMPARE(osd.callsign(), QStringLiteral("DUAL3"));
    osd.selectVehicle(nullptr);
    QVERIFY(osd.callsign().isEmpty());
}

void HerelinkOsdTest::timerPolicy()
{
    HerelinkOsd osd;
    osd.advanceTimer(true, false, 1000);
    QVERIFY(!osd._sessionKnown);
    osd.advanceTimer(true, true, 2000);
    osd.advanceTimer(true, true, 5000);
    QCOMPARE(osd._armedSeconds, 3.0);
    osd.advanceTimer(true, false, 6000);
    osd.advanceTimer(true, false, 9000);
    QCOMPARE(osd._armedSeconds, 3.0);
    osd.advanceTimer(true, true, 10000);
    QCOMPARE(osd._armedSeconds, 0.0);
    osd.advanceTimer(false, true, 11000);
    QVERIFY(!osd._sessionKnown);
    osd.advanceTimer(true, true, 15000);
    QCOMPARE(osd._armedSeconds, 0.0);
}

void HerelinkOsdTest::telemetryFreshness()
{
    Vehicle vehicle(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_QUADROTOR);
    HerelinkOsd osd;
    osd.selectVehicle(&vehicle);
    osd._heartbeat = HerelinkOsd::now();
    osd._altitude = HerelinkOsd::now();
    osd._climb = HerelinkOsd::now();
    osd._altitudeValid = osd._climbValid = true;
    vehicle.altitudeRelative()->setRawValue(1.25);
    vehicle.climbRate()->setRawValue(-0.5);
    osd.refresh();
    QVERIFY(osd.data().value("banner").toString().isEmpty());
    vehicle.setPrearmError(QStringLiteral("PreArm: Safety switch"));
    osd.refresh();
    QCOMPARE(osd.data().value("banner").toString(), QStringLiteral("PreArm: Safety switch"));
    QCOMPARE(osd.data().value("tone").toString(), QStringLiteral("warning"));
    vehicle.setPrearmError(QString());
    osd.refresh();
    QVERIFY(osd.data().value("banner").toString().isEmpty());
    QCOMPARE(osd.data().value("altitude").toString(), QStringLiteral("1.3"));
    QCOMPARE(osd.data().value("climb").toString(), QStringLiteral("-0.5"));
    vehicle.throttlePct()->setRawValue(37);
    osd.refresh();
    QCOMPARE(osd.data().value("motorOutput").toString(), QStringLiteral("37"));
    osd._altitude = HerelinkOsd::now() - 4000;
    osd.refresh();
    QCOMPARE(osd.data().value("altitude").toString(), QStringLiteral("—"));
    osd._heartbeat = HerelinkOsd::now() - 4000;
    osd.refresh();
    QVERIFY(!osd.data().value("connected").toBool());
    QCOMPARE(osd.data().value("climb").toString(), QStringLiteral("—"));
    QCOMPARE(osd.data().value("motorOutput").toString(), QStringLiteral("—"));
}

void HerelinkOsdTest::batteryAndCamera()
{
    QCOMPARE(HerelinkOsd::cellVoltage(22.14, 6), QStringLiteral("3.69"));
    QCOMPARE(HerelinkOsd::cellVoltage(22.14, 0), QStringLiteral("—"));
    QCOMPARE(HerelinkOsd::cellVoltage(qQNaN(), 6), QStringLiteral("—"));
    QCOMPARE(HerelinkOsd::cellVoltage(0, 6), QStringLiteral("—"));
    Vehicle vehicle(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_QUADROTOR);
    HerelinkOsd osd;
    osd.selectVehicle(&vehicle);
    osd._heartbeat = HerelinkOsd::now();
    auto *const battery = new BatteryFactGroup(0, vehicle.batteries());
    vehicle.batteries()->append(battery);
    battery->voltage()->setRawValue(14.39);
    battery->current()->setRawValue(0.1);
    osd._battery = HerelinkOsd::now();
    osd.refresh();
    QCOMPARE(osd.data().value("voltage").toString(), QStringLiteral("14.39"));
    QCOMPARE(osd.data().value("voltageUnit").toString(), QStringLiteral("V pack"));
    QCOMPARE(osd.data().value("current").toString(), QStringLiteral("0.1"));
    osd._battery = HerelinkOsd::now() - 6000;
    osd.refresh();
    QCOMPARE(osd.data().value("voltage").toString(), QStringLiteral("—"));
    QCOMPARE(osd._camera, QStringLiteral("CAM —"));
    auto commandBytes = [&vehicle](int relay, int value) {
        mavlink_message_t message{};
        mavlink_msg_command_long_pack(255, 190, &message, static_cast<uint8_t>(vehicle.id()),
            static_cast<uint8_t>(vehicle.defaultComponentId()), MAV_CMD_DO_SET_RELAY, 0,
            static_cast<float>(relay), static_cast<float>(value), 0, 0, 0, 0, 0);
        uint8_t buffer[MAVLINK_MAX_PACKET_LEN]{};
        const uint16_t size = mavlink_msg_to_send_buffer(buffer, &message);
        return QByteArray(reinterpret_cast<const char *>(buffer), size);
    };
    // Observe synthetic bytes only. They are never sent to a link.
    osd.observeSent(commandBytes(0, 0));
    QCOMPARE(osd._camera, QStringLiteral("CAM —"));
    const QByteArray off = commandBytes(1, 0);
    osd.observeSent(off.first(4));
    osd.observeSent(off.mid(4));
    QCOMPARE(osd._camera, QStringLiteral("CAM1 · CMD"));
    osd.observeSent(commandBytes(1, 1));
    QCOMPARE(osd._camera, QStringLiteral("CAM2 · CMD"));
    mavlink_relay_status_t relay{};
    relay.present = 2;
    relay.on = 0;
    mavlink_message_t relayMessage{};
    mavlink_msg_relay_status_encode(static_cast<uint8_t>(vehicle.id()),
        static_cast<uint8_t>(vehicle.defaultComponentId()), &relayMessage, &relay);
    osd.receive(relayMessage);
    QCOMPARE(osd._camera, QStringLiteral("CAM1 · FC"));
    relay.on = 2;
    mavlink_msg_relay_status_encode(static_cast<uint8_t>(vehicle.id()),
        static_cast<uint8_t>(vehicle.defaultComponentId()), &relayMessage, &relay);
    osd.receive(relayMessage);
    QCOMPARE(osd._camera, QStringLiteral("CAM2 · FC"));
    auto statusText = [&vehicle, &osd](uint8_t severity, const char *text) {
        mavlink_statustext_t status{};
        status.severity = severity;
        qstrncpy(status.text, text, sizeof(status.text));
        mavlink_message_t message{};
        mavlink_msg_statustext_encode(static_cast<uint8_t>(vehicle.id()),
            static_cast<uint8_t>(vehicle.defaultComponentId()), &message, &status);
        osd.receive(message);
    };
    statusText(MAV_SEVERITY_CRITICAL, "Critical OSD fixture");
    statusText(MAV_SEVERITY_WARNING, "Lower priority fixture");
    QCOMPARE(osd._warning, QStringLiteral("Critical OSD fixture"));
    osd.refresh();
    QCOMPARE(osd.data().value("tone").toString(), QStringLiteral("critical"));
    osd._warningAt = HerelinkOsd::now() - 13000;
    statusText(MAV_SEVERITY_WARNING, "New warning after expiry");
    QCOMPARE(osd._warning, QStringLiteral("New warning after expiry"));
    osd.selectVehicle(nullptr);
    QCOMPARE(osd._camera, QStringLiteral("CAM —"));
}

void HerelinkOsdTest::joystickSnapshots()
{
    QScopedPointer<MockJoystick> joystick(MockJoystick::create(QStringLiteral("OSD read-only test")));
    QVERIFY(joystick);
    HerelinkOsd osd;
    osd.selectJoystick(joystick.data());
    emit joystick->axisValues(1, -1, 0.5f, 0);
    QTRY_COMPARE_WITH_TIMEOUT(osd._roll, 1.0f, 1000);
    QCOMPARE(osd._pitch, -1.0f);
    QCOMPARE(osd._yaw, 0.5f);
    QCOMPARE(osd._throttle, 0.0f);
    osd.refresh();
    QVERIFY(!osd.data().value("sticks").toBool()); // Not enabled/polling for a vehicle.
    emit joystick->axisValues(-1, 1, -0.5f, 1);
    osd.selectJoystick(nullptr);
    QCoreApplication::processEvents();
    QCOMPARE(osd._axes, qint64(-1)); // Discard the queued sample from the old device.
}
