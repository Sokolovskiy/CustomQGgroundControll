#include "MavCommandQueue.h"

#include <QtCore/QTimer>

#include "FirmwarePlugin.h"
#include "MAVLinkLib.h"
#include "MAVLinkProtocol.h"
#include "MissionCommandTree.h"
#include "AppMessages.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "MultiVehicleManager.h"

#include <cmath>

#ifdef QT_DEBUG
#include "MockLink.h"
#endif

QGC_LOGGING_CATEGORY(MavCommandQueueLog, "Vehicle.MavCommandQueue")

MavCommandQueue::MavCommandQueue(Vehicle* vehicle)
    : QObject(vehicle)
    , _vehicle(vehicle)
{
    _responseCheckTimer.setSingleShot(false);
    _responseCheckTimer.setInterval(_responseCheckIntervalMSecs());
    _responseCheckTimer.start();
    connect(&_responseCheckTimer, &QTimer::timeout, this, &MavCommandQueue::_responseTimeoutCheck);
}

MavCommandQueue::~MavCommandQueue()
{
    _responseCheckTimer.stop();
    _responseCheckTimer.disconnect();
}

void MavCommandQueue::stop()
{
    qCDebug(MavCommandQueueLog) << "stop - clearing pending commands and halting response timer";
    _stopped = true;
    _responseCheckTimer.stop();
    _responseCheckTimer.disconnect();
    _list.clear();
}

void MavCommandQueue::sendCommand(int compId, MAV_CMD command, bool showError, float param1, float param2, float param3, float param4, float param5, float param6, float param7)
{
    sendWorker(false,                   // commandInt
               showError,
               nullptr,                 // no handlers
               compId,
               command,
               MAV_FRAME_GLOBAL,
               param1, param2, param3, param4, param5, param6, param7);
}

void MavCommandQueue::sendCommandDelayed(int compId, MAV_CMD command, bool showError, int milliseconds, float param1, float param2, float param3, float param4, float param5, float param6, float param7)
{
    QTimer::singleShot(milliseconds, this, [=, this] {
        sendCommand(compId, command, showError, param1, param2, param3, param4, param5, param6, param7);
    });
}

void MavCommandQueue::sendCommandInt(int compId, MAV_CMD command, MAV_FRAME frame, bool showError, float param1, float param2, float param3, float param4, double param5, double param6, float param7)
{
    sendWorker(true,                    // commandInt
               showError,
               nullptr,                 // no handlers
               compId,
               command,
               frame,
               param1, param2, param3, param4, param5, param6, param7);
}

void MavCommandQueue::sendCommandWithHandler(const MavCmdAckHandlerInfo_t* ackHandlerInfo, int compId, MAV_CMD command, float param1, float param2, float param3, float param4, float param5, float param6, float param7)
{
    sendWorker(false,                   // commandInt
               false,                   // showError
               ackHandlerInfo,
               compId,
               command,
               MAV_FRAME_GLOBAL,
               param1, param2, param3, param4, param5, param6, param7);
}

void MavCommandQueue::sendCommandIntWithHandler(const MavCmdAckHandlerInfo_t* ackHandlerInfo, int compId, MAV_CMD command, MAV_FRAME frame, float param1, float param2, float param3, float param4, double param5, double param6, float param7)
{
    sendWorker(true,                    // commandInt
               false,                   // showError
               ackHandlerInfo,
               compId,
               command,
               frame,
               param1, param2, param3, param4, param5, param6, param7);
}

namespace {

struct LambdaFallbackHandlerData {
    Vehicle*              vehicle;
    bool                  showError;
    std::function<void()> unsupportedLambda;
};

void lambdaFallbackResultHandler(void* resultHandlerData, int /*compId*/, const mavlink_command_ack_t& ack, VehicleTypes::MavCmdResultFailureCode_t /*failureCode*/)
{
    auto* data          = static_cast<LambdaFallbackHandlerData*>(resultHandlerData);
    auto* instanceData  = data->vehicle->firmwarePluginInstanceData();

    switch (ack.result) {
    case MAV_RESULT_ACCEPTED:
        instanceData->setCommandSupported(MAV_CMD(ack.command), FirmwarePluginInstanceData::CommandSupportedResult::SUPPORTED);
        break;
    case MAV_RESULT_UNSUPPORTED:
        instanceData->setCommandSupported(MAV_CMD(ack.command), FirmwarePluginInstanceData::CommandSupportedResult::UNSUPPORTED);
        data->unsupportedLambda();
        break;
    default:
        if (data->showError) {
            MavCommandQueue::showCommandAckError(ack);
        }
        break;
    }

    delete data;
}

} // namespace

void MavCommandQueue::sendCommandWithLambdaFallbackWorker(std::function<void()> lambda, bool commandInt, int compId, MAV_CMD command, MAV_FRAME frame, bool showError, float param1, float param2, float param3, float param4, double param5, double param6, float param7)
{
    auto* instanceData = _vehicle->firmwarePluginInstanceData();

    switch (instanceData->getCommandSupported(command)) {
    case FirmwarePluginInstanceData::CommandSupportedResult::UNSUPPORTED:
        lambda();
        break;
    case FirmwarePluginInstanceData::CommandSupportedResult::SUPPORTED:
        if (commandInt) {
            sendCommandInt(compId, command, frame, showError, param1, param2, param3, param4, param5, param6, param7);
        } else {
            sendCommand(compId, command, showError, param1, param2, param3, param4, param5, param6, param7);
        }
        break;
    case FirmwarePluginInstanceData::CommandSupportedResult::UNKNOWN: {
        auto* data = new LambdaFallbackHandlerData { _vehicle, showError, std::move(lambda) };
        const MavCmdAckHandlerInfo_t handlerInfo {
            /* .resultHandler         = */ &lambdaFallbackResultHandler,
            /* .resultHandlerData     = */ data,
            /* .progressHandler       = */ nullptr,
            /* .progressHandlerData   = */ nullptr,
        };
        if (commandInt) {
            sendCommandIntWithHandler(&handlerInfo, compId, command, frame, param1, param2, param3, param4, param5, param6, param7);
        } else {
            sendCommandWithHandler(&handlerInfo, compId, command, param1, param2, param3, param4, param5, param6, param7);
        }
        break;
    }
    }
}

void MavCommandQueue::sendCommandWithLambdaFallback(std::function<void()> lambda, int compId, MAV_CMD command, bool showError, float param1, float param2, float param3, float param4, float param5, float param6, float param7)
{
    sendCommandWithLambdaFallbackWorker(std::move(lambda), false /* commandInt */, compId, command, MAV_FRAME_GLOBAL, showError, param1, param2, param3, param4, param5, param6, param7);
}

void MavCommandQueue::sendCommandIntWithLambdaFallback(std::function<void()> lambda, int compId, MAV_CMD command, MAV_FRAME frame, bool showError, float param1, float param2, float param3, float param4, double param5, double param6, float param7)
{
    sendCommandWithLambdaFallbackWorker(std::move(lambda), true /* commandInt */, compId, command, frame, showError, param1, param2, param3, param4, param5, param6, param7);
}

bool MavCommandQueue::isPending(int targetCompId, MAV_CMD command) const
{
    return findEntryIndex(targetCompId, command) != -1;
}

int MavCommandQueue::findEntryIndex(int targetCompId, MAV_CMD command) const
{
    for (int i = 0; i < _list.count(); i++) {
        const MavCommandListEntry_t& entry = _list[i];
        if (entry.targetCompId == targetCompId && entry.command == command) {
            return i;
        }
    }
    return -1;
}

int MavCommandQueue::_responseCheckIntervalMSecs()
{
    // Use shorter check interval during unit tests for faster test execution
    return QGC::runningUnitTests() ? 50 : 500;
}

int MavCommandQueue::_ackTimeoutMSecs()
{
    // Use shorter ack timeout during unit tests for faster test execution
    return QGC::runningUnitTests() ? kTestAckTimeoutMs : 1200;
}

bool MavCommandQueue::_shouldRetry(MAV_CMD command)
{
    switch (command) {
#ifdef QT_DEBUG
    // These MockLink commands should be retried so we can create unit tests to test retry code
    case MockLink::MAV_CMD_MOCKLINK_ALWAYS_RESULT_ACCEPTED:
    case MockLink::MAV_CMD_MOCKLINK_ALWAYS_RESULT_FAILED:
    case MockLink::MAV_CMD_MOCKLINK_SECOND_ATTEMPT_RESULT_ACCEPTED:
    case MockLink::MAV_CMD_MOCKLINK_SECOND_ATTEMPT_RESULT_FAILED:
    case MockLink::MAV_CMD_MOCKLINK_NO_RESPONSE:
        return true;
#endif

        // In general we should not retry any commands. This is for safety reasons. For example you don't want an ARM command
        // to timeout with no response over a noisy link twice and then suddenly have the third try work 6 seconds later. At that
        // point the user could have walked up to the vehicle to see what is going wrong.
        //
        // We do retry commands which are part of the initial vehicle connect sequence. This makes this process work better over noisy
        // links where commands could be lost. Also these commands tend to just be requesting status so if they end up being delayed
        // there are no safety concerns that could occur.
    case MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES:
    case MAV_CMD_REQUEST_MESSAGE:
    case MAV_CMD_PREFLIGHT_STORAGE:
    case MAV_CMD_RUN_PREARM_CHECKS:
        return true;

    default:
        return false;
    }
}

bool MavCommandQueue::_canBeDuplicated(MAV_CMD command)
{
    // For some commands we don't care about response as much as we care about sending them regularly.
    // This test avoids commands not being sent due to an ACK not being received yet.
    // MOTOR_TEST in ardusub is a case where we need a constant stream of commands so it doesn't time out.
    switch (command) {
    case MAV_CMD_DO_MOTOR_TEST:
    case MAV_CMD_SET_MESSAGE_INTERVAL:
        return true;
    default:
        return false;
    }
}

QString MavCommandQueue::_formatCommand(MAV_CMD command, float param1)
{
    QString rawName = MissionCommandTree::instance()->rawName(command);
    QString friendlyName = MissionCommandTree::instance()->friendlyName(command);
    QString commandStr = friendlyName.isEmpty() ? rawName : QStringLiteral("%1 (%2)").arg(friendlyName, rawName);

    if (command == MAV_CMD_REQUEST_MESSAGE) {
        const mavlink_message_info_t* info = mavlink_get_message_info_by_id(static_cast<int>(param1));
        if (info) {
            commandStr += QStringLiteral(" [%1]").arg(info->name);
        }
    }

    return commandStr;
}

void MavCommandQueue::sendWorker(bool commandInt, bool showError,
                                 const MavCmdAckHandlerInfo_t* ackHandlerInfo,
                                 int targetCompId, MAV_CMD command, MAV_FRAME frame,
                                 float param1, float param2, float param3, float param4, double param5, double param6, float param7)
{
    if (_stopped || !_vehicle) {
        return;
    }

    // Even a rejected newer request must invalidate an older ARM/DISARM repeat.
    if (command == MAV_CMD_COMPONENT_ARM_DISARM) {
        ++_armRetryGeneration;
    }

    bool reliableSelection = false;
#ifdef QGC_CUSTOM_BUILD
    // Only absolute mode selection and this build's camera Relay2 OFF/ON.
    // ARM/DISARM uses a separate short-window policy below.
    reliableSelection = !commandInt && !ackHandlerInfo
        && targetCompId == _vehicle->defaultComponentId()
        && _vehicle == MultiVehicleManager::instance()->activeVehicle()
        && param3 == 0 && param4 == 0 && param5 == 0 && param6 == 0 && param7 == 0
        && ((command == MAV_CMD_DO_SET_MODE && param1 == static_cast<float>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED)
             && std::isfinite(param2) && param2 >= 0 && param2 <= 16777215
             && std::floor(param2) == param2)
            || (command == MAV_CMD_DO_SET_RELAY && param1 == 1 && (param2 == 0 || param2 == 1)));
#endif
    const int selectionKey = reliableSelection ? targetCompId * 65536 + static_cast<int>(command) : 0;
    const quint64 selectionGeneration = reliableSelection ? ++_selectionGenerations[selectionKey] : 0;
    const int pendingSelection = findEntryIndex(targetCompId, command);
    if (reliableSelection && pendingSelection >= 0 && _list[pendingSelection].reliableSelection) {
        auto &pending = _list[pendingSelection];
        const auto requestedLink = _vehicle->vehicleLinkManager()->primaryLink().lock();
        const QPointer<LinkInterface> link = requestedLink.get();
        const bool armed = _vehicle->armed();
        QElapsedTimer requestedAt;
        requestedAt.start();
        // Keep the previous ACK slot until it terminates. Replace only the
        // deferred intent, so ACKs cannot immediately complete another request.
        pending.maxTries = pending.tryCount;
        pending.nextSelection = [this, link, armed, requestedAt, showError, targetCompId, vehicleId = _vehicle->id(),
                                 selectionKey, selectionGeneration,
                                 command, frame, param1, param2, param3, param4, param5, param6, param7] {
            if (_selectionGenerations.value(selectionKey) != selectionGeneration) {
                qCDebug(MavCommandQueueLog) << "Discarding superseded selection callback" << command;
                return;
            }
            const auto currentLink = _vehicle ? _vehicle->vehicleLinkManager()->primaryLink().lock() : nullptr;
            if (_stopped || !_vehicle || !link || currentLink.get() != link
                || _vehicle != MultiVehicleManager::instance()->activeVehicle()
                || _vehicle->vehicleLinkManager()->communicationLost()
                || _vehicle->armed() != armed || requestedAt.elapsed() > 2000) {
                qCDebug(MavCommandQueueLog) << "Discarding expired or disconnected latest selection" << command;
                QPointer<MavCommandQueue> guard(this);
                emit commandResult(vehicleId, targetCompId, command, MAV_RESULT_CANCELLED, MavCmdResultCommandResultOnly);
                if (!guard) { return; }
                if (showError && !_stopped) {
                    QGC::showAppMessage(tr("Pending selection expired or connection/state changed. Select it again when ready."));
                }
                return;
            }
            sendWorker(false, showError, nullptr, targetCompId, command, frame,
                       param1, param2, param3, param4, param5, param6, param7);
        };
        qCDebug(MavCommandQueueLog) << "Latest selection queued; previous selection will not retry"
                                  << command << param1 << param2;
        return;
    }

    // We can't send commands to compIdAll using this method. The reason being that we would get responses back possibly from multiple components
    // which this code can't handle.
    // We also can't send the majority of commands again if we are already waiting for a response from that same command. If we did that we would not be able to discern
    // which ack was associated with which command.
    if ((targetCompId == MAV_COMP_ID_ALL) || (isPending(targetCompId, command) && !_canBeDuplicated(command))) {
        bool    compIdAll       = targetCompId == MAV_COMP_ID_ALL;
        QString rawCommandName  = MissionCommandTree::instance()->rawName(command);

        qCDebug(MavCommandQueueLog) << QStringLiteral("sendWorker failing %1").arg(compIdAll ? "MAV_COMP_ID_ALL not supported" : "duplicate command") << rawCommandName << param1 << param2 << param3 << param4 << param5 << param6 << param7;

        MavCmdResultFailureCode_t failureCode = compIdAll ? MavCmdResultCommandResultOnly : MavCmdResultFailureDuplicateCommand;
        if (ackHandlerInfo && ackHandlerInfo->resultHandler) {
            mavlink_command_ack_t ack = {};
            ack.result = MAV_RESULT_FAILED;
            (*ackHandlerInfo->resultHandler)(ackHandlerInfo->resultHandlerData, targetCompId, ack, failureCode);
        } else {
            emit commandResult(_vehicle->id(), targetCompId, command, MAV_RESULT_FAILED, failureCode);
        }
        if (showError) {
            QGC::showAppMessage(tr("Unable to send command: %1.").arg(compIdAll ? tr("Internal error - MAV_COMP_ID_ALL not supported") : tr("Waiting on previous response to same command.")));
        }
        return;
    }

    SharedLinkInterfacePtr sharedLink = _vehicle->vehicleLinkManager()->primaryLink().lock();
    if (!sharedLink) {
        qCDebug(MavCommandQueueLog) << "sendWorker: primary link gone!";

        const MavCmdResultFailureCode_t failureCode = MavCmdResultFailureNoResponseToCommand;
        if (ackHandlerInfo && ackHandlerInfo->resultHandler) {
            mavlink_command_ack_t ack = {};
            ack.command = command;
            ack.result = MAV_RESULT_FAILED;
            (*ackHandlerInfo->resultHandler)(ackHandlerInfo->resultHandlerData, targetCompId, ack, failureCode);
        } else {
            emit commandResult(_vehicle->id(), targetCompId, command, MAV_RESULT_FAILED, failureCode);
        }

        if (showError) {
            QGC::showAppMessage(tr("Unable to send command: Vehicle is not connected."));
        }
        return;
    }

    MavCommandListEntry_t entry;
    entry.useCommandInt     = commandInt;
    entry.targetCompId      = targetCompId;
    entry.command           = command;
    entry.frame             = frame;
    entry.showError         = showError;
    entry.ackHandlerInfo    = {};
    if (ackHandlerInfo) {
        entry.ackHandlerInfo = *ackHandlerInfo;
    }
    entry.rgParam1          = param1;
    entry.rgParam2          = param2;
    entry.rgParam3          = param3;
    entry.rgParam4          = param4;
    entry.rgParam5          = param5;
    entry.rgParam6          = param6;
    entry.rgParam7          = param7;
    entry.maxTries          = _shouldRetry(command) ? kMaxRetryCount : 1;
    entry.ackTimeoutMSecs   = sharedLink->linkConfiguration()->isHighLatency() ? _ackTimeoutMSecsHighLatency : _ackTimeoutMSecs();
    entry.reliableSelection = reliableSelection && !sharedLink->linkConfiguration()->isHighLatency();
    if (entry.reliableSelection) {
        entry.maxTries = 3;
        entry.requestedLink = sharedLink.get();
        entry.requestedArmed = _vehicle->armed();
    }
#ifdef QGC_CUSTOM_BUILD
    const bool shortArmRetry = command == MAV_CMD_COMPONENT_ARM_DISARM
        && !commandInt && !ackHandlerInfo && _vehicle->apmFirmware()
        && targetCompId == _vehicle->defaultComponentId()
        && _vehicle == MultiVehicleManager::instance()->activeVehicle()
        && sharedLink->isConnected() && !sharedLink->linkConfiguration()->isHighLatency()
        && !_vehicle->vehicleLinkManager()->communicationLost()
        && (param1 == 0 || param1 == 1) && _vehicle->armed() != (param1 == 1)
        && param2 == 0 && param3 == 0 && param4 == 0 && param5 == 0 && param6 == 0 && param7 == 0;
    if (shortArmRetry) {
        if (!_armRetryGuardsConnected) {
            // Invalidate on transitions even if state returns before the timer fires.
            const auto invalidate = [this] { ++_armRetryGeneration; };
            connect(_vehicle, &Vehicle::armedChanged, this, invalidate);
            connect(MultiVehicleManager::instance(), &MultiVehicleManager::activeVehicleChanged, this, invalidate);
            connect(_vehicle->vehicleLinkManager(), &VehicleLinkManager::primaryLinkChanged, this, invalidate);
            connect(_vehicle->vehicleLinkManager(), &VehicleLinkManager::communicationLostChanged, this, invalidate);
            _armRetryGuardsConnected = true;
        }
        disconnect(_armRetryLinkConnection);
        _armRetryLinkConnection = connect(sharedLink.get(), &LinkInterface::disconnected, this,
                                         [this] { ++_armRetryGeneration; });
        entry.armRetryGeneration = _armRetryGeneration;
        entry.requestedLink = sharedLink.get();
        entry.requestedArmed = _vehicle->armed();
        entry.maxTries = 2;
    }
#endif
    entry.elapsedTimer.start();

    qCDebug(MavCommandQueueLog) << "Sending" << _formatCommand(command, param1) << "param1-7:" << command << param1 << param2 << param3 << param4 << param5 << param6 << param7;

    _list.append(entry);
    QPointer<MavCommandQueue> guard(this);
    _sendFromList(_list.count() - 1);
    if (guard && entry.armRetryGeneration != 0) {
        QTimer::singleShot(100, Qt::PreciseTimer, this,
            [this, targetCompId, generation = entry.armRetryGeneration] {
                const int index = findEntryIndex(targetCompId, MAV_CMD_COMPONENT_ARM_DISARM);
                if (index < 0 || _list[index].armRetryGeneration != generation) { return; }
                if (!_armRetryAllowed(_list[index])) {
                    qCDebug(MavCommandQueueLog) << "Short ARM/DISARM repeat cancelled or expired";
                    return;
                }
                qCInfo(MavCommandQueueLog) << "Short ARM/DISARM repeat param1/elapsed ms:"
                    << _list[index].rgParam1 << _list[index].armRetryElapsed.elapsed();
                _sendFromList(index);
            });
    }
}

bool MavCommandQueue::_armRetryAllowed(const MavCommandListEntry_t &entry) const
{
    if (_stopped || !_vehicle || entry.armRetryGeneration == 0
        || entry.armRetryGeneration != _armRetryGeneration || entry.tryCount != 1
        || !entry.armRetryElapsed.isValid() || entry.armRetryElapsed.elapsed() >= 250
        || !entry.requestedLink || !entry.requestedLink->isConnected()
        || _vehicle != MultiVehicleManager::instance()->activeVehicle()
        || _vehicle->vehicleLinkManager()->communicationLost()
        || _vehicle->armed() != entry.requestedArmed) {
        return false;
    }
    const auto link = _vehicle->vehicleLinkManager()->primaryLink().lock();
    return link && link.get() == entry.requestedLink
        && !link->linkConfiguration()->isHighLatency();
}

void MavCommandQueue::_sendFromList(int index)
{
    MavCommandListEntry_t commandEntry = _list[index];

    if (commandEntry.armRetryGeneration != 0 && commandEntry.tryCount > 0
        && !_armRetryAllowed(commandEntry)) {
        // The normal ACK timeout may report failure, but must never send a late ARM.
        _list[index].maxTries = _list[index].tryCount;
        commandEntry.maxTries = _list[index].maxTries;
    }
    if (commandEntry.reliableSelection && commandEntry.tryCount > 0) {
        const auto currentLink = _vehicle->vehicleLinkManager()->primaryLink().lock();
        if (!commandEntry.requestedLink || currentLink.get() != commandEntry.requestedLink
            || _vehicle != MultiVehicleManager::instance()->activeVehicle()
            || _vehicle->vehicleLinkManager()->communicationLost()
            || _vehicle->armed() != commandEntry.requestedArmed) {
            // Fall through to the standard terminal failure path, without sending.
            _list[index].maxTries = _list[index].tryCount;
            commandEntry.maxTries = _list[index].maxTries;
        }
    }

    QString rawCommandName = MissionCommandTree::instance()->rawName(commandEntry.command);
    QString friendlyName = MissionCommandTree::instance()->friendlyName(commandEntry.command);

    if (++_list[index].tryCount > commandEntry.maxTries) {
        if (commandEntry.nextSelection) {
            _list.removeAt(index);
            QPointer<MavCommandQueue> guard(this);
            emit commandResult(_vehicle->id(), commandEntry.targetCompId, commandEntry.command,
                               MAV_RESULT_CANCELLED, MavCmdResultCommandResultOnly);
            if (guard && !_stopped) {
                QTimer::singleShot(0, this, std::move(commandEntry.nextSelection));
            }
            return;
        }
        QString logMsg = QStringLiteral("Giving up sending command after max retries: %1").arg(rawCommandName);

        // For REQUEST_MESSAGE commands, also log which message was being requested
        if (commandEntry.command == MAV_CMD_REQUEST_MESSAGE) {
            int requestedMsgId = static_cast<int>(commandEntry.rgParam1);
            const mavlink_message_info_t *info = mavlink_get_message_info_by_id(requestedMsgId);
            logMsg += QStringLiteral(" requesting: %1").arg(info ? info->name : QString::number(requestedMsgId));
        }

        qCWarning(MavCommandQueueLog) << logMsg;

        _list.removeAt(index);
        if (commandEntry.ackHandlerInfo.resultHandler) {
            mavlink_command_ack_t ack = {};
            ack.result = MAV_RESULT_FAILED;
            (*commandEntry.ackHandlerInfo.resultHandler)(commandEntry.ackHandlerInfo.resultHandlerData, commandEntry.targetCompId, ack, MavCmdResultFailureNoResponseToCommand);
        } else {
            emit commandResult(_vehicle->id(), commandEntry.targetCompId, commandEntry.command, MAV_RESULT_FAILED, MavCmdResultFailureNoResponseToCommand);
        }
        if (commandEntry.showError) {
            QGC::showAppMessage(tr("Vehicle did not respond to command: %1").arg(friendlyName));
        }
        return;
    }

    if (commandEntry.tryCount > 1 && !_vehicle->px4Firmware() && commandEntry.command == MAV_CMD_START_RX_PAIR) {
        // The implementation of this command comes from the IO layer and is shared across stacks. So for other firmwares
        // we aren't really sure whether they are correct or not.
        return;
    }

    qCDebug(MavCommandQueueLog) << "Sending" << _formatCommand(commandEntry.command, commandEntry.rgParam1)
                                << "tryCount:param1-7" << commandEntry.tryCount << commandEntry.rgParam1 << commandEntry.rgParam2 << commandEntry.rgParam3 << commandEntry.rgParam4 << commandEntry.rgParam5 << commandEntry.rgParam6 << commandEntry.rgParam7;

    SharedLinkInterfacePtr sharedLink = _vehicle->vehicleLinkManager()->primaryLink().lock();
    if (!sharedLink) {
        qCDebug(MavCommandQueueLog) << "_sendFromList: primary link gone!";
        return;
    }

    mavlink_message_t msg;

    if (commandEntry.useCommandInt) {
        mavlink_command_int_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.target_system =     _vehicle->id();
        cmd.target_component =  commandEntry.targetCompId;
        cmd.command =           commandEntry.command;
        cmd.frame =             commandEntry.frame;
        cmd.param1 =            commandEntry.rgParam1;
        cmd.param2 =            commandEntry.rgParam2;
        cmd.param3 =            commandEntry.rgParam3;
        cmd.param4 =            commandEntry.rgParam4;
        cmd.x =                 commandEntry.frame == MAV_FRAME_MISSION ? commandEntry.rgParam5 : commandEntry.rgParam5 * 1e7;
        cmd.y =                 commandEntry.frame == MAV_FRAME_MISSION ? commandEntry.rgParam6 : commandEntry.rgParam6 * 1e7;
        cmd.z =                 commandEntry.rgParam7;
        mavlink_msg_command_int_encode_chan(MAVLinkProtocol::instance()->getSystemId(),
                                            MAVLinkProtocol::getComponentId(),
                                            sharedLink->mavlinkChannel(),
                                            &msg,
                                            &cmd);
    } else {
        mavlink_command_long_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.target_system =     _vehicle->id();
        cmd.target_component =  commandEntry.targetCompId;
        cmd.command =           commandEntry.command;
        // Use the incremented queue counter; commandEntry is a pre-increment snapshot.
        // MAVLink confirmation starts at zero and increments on each resend.
        cmd.confirmation =      static_cast<uint8_t>(qMin(_list[index].tryCount - 1, 255));
        cmd.param1 =            commandEntry.rgParam1;
        cmd.param2 =            commandEntry.rgParam2;
        cmd.param3 =            commandEntry.rgParam3;
        cmd.param4 =            commandEntry.rgParam4;
        cmd.param5 =            static_cast<float>(commandEntry.rgParam5);
        cmd.param6 =            static_cast<float>(commandEntry.rgParam6);
        cmd.param7 =            commandEntry.rgParam7;
        mavlink_msg_command_long_encode_chan(MAVLinkProtocol::instance()->getSystemId(),
                                             MAVLinkProtocol::getComponentId(),
                                             sharedLink->mavlinkChannel(),
                                             &msg,
                                             &cmd);
    }

    if (commandEntry.reliableSelection) {
        _list[index].elapsedTimer.restart();
    }
    if (commandEntry.armRetryGeneration != 0 && commandEntry.tryCount > 0
        && !_armRetryAllowed(commandEntry)) {
        qCDebug(MavCommandQueueLog) << "Short ARM/DISARM repeat cancelled before dispatch";
        return;
    }
    if (commandEntry.armRetryGeneration != 0 && commandEntry.tryCount == 0) {
        _list[index].armRetryElapsed.start();
    }
    _vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
}

void MavCommandQueue::_responseTimeoutCheck()
{
    if (_list.isEmpty()) {
        return;
    }

    // Walk the list backwards since _sendFromList can remove entries. Give-up result
    // handlers invoked from _sendFromList can also re-enter the queue and remove or
    // clear entries (e.g. closing the vehicle from within the callback), so re-validate
    // the stop flag and index on every iteration.
    for (int i = _list.count() - 1; i >= 0; i--) {
        if (_stopped) {
            return;
        }
        if (i >= _list.count()) {
            i = _list.count();  // Handler removed entries below us, clamp (loop decrement follows)
            continue;
        }
        MavCommandListEntry_t& commandEntry = _list[i];
        if (commandEntry.elapsedTimer.elapsed() > commandEntry.ackTimeoutMSecs) {
            // Try sending command again
            _sendFromList(i);
        }
    }
}

void MavCommandQueue::showCommandAckError(const mavlink_command_ack_t& ack)
{
    QString rawName      = MissionCommandTree::instance()->rawName(static_cast<MAV_CMD>(ack.command));
    QString friendlyName = MissionCommandTree::instance()->friendlyName(static_cast<MAV_CMD>(ack.command));
    QString commandStr   = friendlyName.isEmpty() ? rawName : QStringLiteral("%1 (%2)").arg(friendlyName, rawName);

    switch (ack.result) {
    case MAV_RESULT_TEMPORARILY_REJECTED:
        QGC::showAppMessage(tr("%1 command temporarily rejected").arg(commandStr));
        break;
    case MAV_RESULT_DENIED:
        QGC::showAppMessage(tr("%1 command denied").arg(commandStr));
        break;
    case MAV_RESULT_UNSUPPORTED:
        QGC::showAppMessage(tr("%1 command not supported").arg(commandStr));
        break;
    case MAV_RESULT_FAILED:
        QGC::showAppMessage(tr("%1 command failed").arg(commandStr));
        break;
    default:
        // Do nothing
        break;
    }
}

void MavCommandQueue::handleCommandAck(const mavlink_message_t& message, const mavlink_command_ack_t& ack)
{
    int entryIndex = findEntryIndex(message.compid, static_cast<MAV_CMD>(ack.command));
    if (entryIndex == -1) {
        QString rawCommandName = MissionCommandTree::instance()->rawName(static_cast<MAV_CMD>(ack.command));
        qCDebug(MavCommandQueueLog) << "handleCommandAck Ack not in list" << rawCommandName;
        return;
    }

    if (ack.result == MAV_RESULT_IN_PROGRESS) {
        MavCommandListEntry_t commandEntry;
        if (_vehicle->px4Firmware() && ack.command == MAV_CMD_DO_AUTOTUNE_ENABLE) {
            // Hack to support PX4 autotune which does not send final result ack and just sends in progress
            commandEntry = _list.takeAt(entryIndex);
        } else {
            // Command has not completed yet, don't remove
            MavCommandListEntry_t& commandEntryRef = _list[entryIndex];
            commandEntryRef.armRetryGeneration = 0; // Any ACK, including progress, cancels the short repeat.
            commandEntryRef.maxTries = 1;         // Vehicle responded to command so don't retry
            commandEntryRef.elapsedTimer.start(); // We've heard from vehicle, restart elapsed timer for no ack received timeout
            commandEntry = commandEntryRef;
        }

        if (commandEntry.ackHandlerInfo.progressHandler) {
            (*commandEntry.ackHandlerInfo.progressHandler)(commandEntry.ackHandlerInfo.progressHandlerData, message.compid, ack);
        }
        return;
    }

    MavCommandListEntry_t commandEntry = _list.takeAt(entryIndex);
    if (commandEntry.nextSelection) {
        qCDebug(MavCommandQueueLog) << "Previous selection finished; dispatching latest intent" << ack.command;
        QPointer<MavCommandQueue> guard(this);
        emit commandResult(_vehicle->id(), commandEntry.targetCompId, commandEntry.command,
                           MAV_RESULT_CANCELLED, MavCmdResultCommandResultOnly);
        if (guard && !_stopped) {
            QTimer::singleShot(0, this, std::move(commandEntry.nextSelection));
        }
        return;
    }
    if (commandEntry.ackHandlerInfo.resultHandler) {
        (*commandEntry.ackHandlerInfo.resultHandler)(commandEntry.ackHandlerInfo.resultHandlerData, message.compid, ack, MavCmdResultCommandResultOnly);
    } else {
        if (commandEntry.showError) {
            showCommandAckError(ack);
        }
        emit commandResult(_vehicle->id(), message.compid, ack.command, ack.result, MavCmdResultCommandResultOnly);
    }
}

QString MavCommandQueue::failureCodeToString(MavCmdResultFailureCode_t failureCode)
{
    switch (failureCode) {
    case MavCmdResultCommandResultOnly:
        return QStringLiteral("Command Result Only");
    case MavCmdResultFailureNoResponseToCommand:
        return QStringLiteral("No Response To Command");
    case MavCmdResultFailureDuplicateCommand:
        return QStringLiteral("Duplicate Command");
    default:
        return QStringLiteral("Unknown (%1)").arg(failureCode);
    }
}
