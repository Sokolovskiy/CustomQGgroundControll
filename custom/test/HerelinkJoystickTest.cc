#include "HerelinkJoystickTest.h"

#include <QtCore/QSettings>

#include "JoystickSettings.h"
#include "MockJoystick.h"
#include "Vehicle.h"

UT_REGISTER_TEST(HerelinkJoystickTest, TestLabel::Unit, TestLabel::Joystick)

void HerelinkJoystickTest::auxiliaryOverrideWireFormat()
{
    QVERIFY(vehicle());
    QVERIFY(mockLink());
    QList<mavlink_rc_channels_override_t> packets;
    mavlink_message_t parser{};
    mavlink_status_t parserStatus{};
    QObject observer;
    connect(mockLink(), &MockLink::writeBytesQueuedSignal, &observer, [&](const QByteArray& bytes) {
        for (const char byte : bytes) {
            mavlink_message_t message{};
            mavlink_status_t status{};
            if (mavlink_frame_char_buffer(&parser, &parserStatus, static_cast<uint8_t>(byte), &message, &status) ==
                    MAVLINK_FRAMING_OK &&
                message.msgid == MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE) {
                mavlink_rc_channels_override_t channels{};
                mavlink_msg_rc_channels_override_decode(&message, &channels);
                packets.append(channels);
            }
        }
    });

    // Current profile: the servo alone uses Additional Axis 6 -> RC10.
    vehicle()->sendJoystickAuxRcOverrideThreadSafe({1000, 1200, 1400, 1600, 1800, 2000},
                                                   {false, false, false, false, false, true}, true);
    QTRY_COMPARE_WITH_TIMEOUT(packets.size(), 1, TestTimeout::shortMs());
    const auto servo = packets.takeFirst();
    QCOMPARE(servo.target_system, vehicle()->id());
    QCOMPARE(servo.target_component, vehicle()->defaultComponentId());
    QCOMPARE(servo.chan1_raw, UINT16_MAX);
    QCOMPARE(servo.chan2_raw, UINT16_MAX);
    QCOMPARE(servo.chan3_raw, UINT16_MAX);
    QCOMPARE(servo.chan4_raw, UINT16_MAX);
    QCOMPARE(servo.chan5_raw, 0);
    QCOMPARE(servo.chan6_raw, 0);
    QCOMPARE(servo.chan7_raw, 0);
    QCOMPARE(servo.chan8_raw, 0);
    QCOMPARE(servo.chan9_raw, UINT16_MAX - 1);
    QCOMPARE(servo.chan10_raw, 2000);
    QCOMPARE(servo.chan11_raw, 0);
    QCOMPARE(servo.chan18_raw, 0);

    vehicle()->sendJoystickAuxRcOverrideThreadSafe({1000, 1200, 1400, 1600, 1800, 2000},
                                                   {true, true, true, true, true, true}, true);
    QTRY_COMPARE_WITH_TIMEOUT(packets.size(), 1, TestTimeout::shortMs());
    const auto all = packets.takeFirst();
    QCOMPARE(all.chan5_raw, 1000);
    QCOMPARE(all.chan6_raw, 1200);
    QCOMPARE(all.chan7_raw, 1400);
    QCOMPARE(all.chan8_raw, 1600);
    QCOMPARE(all.chan9_raw, 1800);
    QCOMPARE(all.chan10_raw, 2000);

    vehicle()->sendJoystickAuxRcOverrideThreadSafe({}, {}, false);
    QTRY_COMPARE_WITH_TIMEOUT(packets.size(), 1, TestTimeout::shortMs());
    const auto release = packets.takeFirst();
    QCOMPARE(release.chan1_raw, UINT16_MAX);
    QCOMPARE(release.chan4_raw, UINT16_MAX);
    QCOMPARE(release.chan5_raw, 0);
    QCOMPARE(release.chan6_raw, 0);
    QCOMPARE(release.chan7_raw, 0);
    QCOMPARE(release.chan8_raw, 0);
    QCOMPARE(release.chan9_raw, UINT16_MAX - 1);
    QCOMPARE(release.chan10_raw, UINT16_MAX - 1);
}

void HerelinkJoystickTest::profilePersistence()
{
    const QString name = QStringLiteral("OpenTX TX16SMK3 Joystick #2");
    const QString group = QStringLiteral("JoystickSettingsV2/") + name;
    QSettings settings;
    settings.remove(group);
    {
        QScopedPointer<MockJoystick> joystick(MockJoystick::create(name, 5, 7, 0));
        QVERIFY(joystick);
        const QStringList actions = {QStringLiteral("Altitude Hold"), QStringLiteral("Position Hold"),
                                     QStringLiteral("Land"), QStringLiteral("Camera Relay OFF"),
                                     QStringLiteral("Camera Relay ON")};
        QCOMPARE(joystick->getButtonAction(0), QStringLiteral("No Action"));
        QCOMPARE(joystick->getButtonAction(1), QStringLiteral("No Action"));
        for (int index = 0; index < actions.size(); ++index) {
            QCOMPARE(joystick->getButtonAction(index + 2), actions[index]);
            QVERIFY(joystick->assignableActionTitles().contains(actions[index]));
            QVERIFY(!joystick->getButtonRepeat(index + 2));
        }
        // Recovery does not mark a new radio calibrated or enable auxiliary channels.
        QVERIFY(!joystick->settings()->calibrated()->rawValue().toBool());
        QVERIFY(!joystick->settings()->enableAdditionalAxis1()->rawValue().toBool());
        joystick->setButtonAction(4, QStringLiteral("No Action"));
    }
    {
        QScopedPointer<MockJoystick> reloaded(MockJoystick::create(name, 5, 7, 0));
        QVERIFY(reloaded);
        QCOMPARE(reloaded->getButtonAction(2), QStringLiteral("Altitude Hold"));
        QCOMPARE(reloaded->getButtonAction(6), QStringLiteral("Camera Relay ON"));
        QCOMPARE(reloaded->getButtonAction(4), QStringLiteral("No Action"));
    }
    settings.remove(group);
    settings.setValue(group + QStringLiteral("/JoystickButtonActionSettingsArray/2/actionName"), QStringLiteral("RTL"));
    {
        QScopedPointer<MockJoystick> customized(MockJoystick::create(name, 5, 7, 0));
        QVERIFY(customized);
        QCOMPARE(customized->getButtonAction(2), QStringLiteral("RTL"));
        QCOMPARE(customized->getButtonAction(3), QStringLiteral("No Action"));
    }
    settings.remove(group);
    {
        QScopedPointer<MockJoystick> otherLayout(MockJoystick::create(name, 6, 7, 0));
        QVERIFY(otherLayout);
        QCOMPARE(otherLayout->getButtonAction(2), QStringLiteral("No Action"));
    }
}
