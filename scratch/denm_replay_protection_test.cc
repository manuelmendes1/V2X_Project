#include "ns3/core-module.h"

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/denm/replay/denm_replay_protection.h"
#include "vanetza_integration/denm/replay/denm_replay_context.h"
#include "vanetza_integration/denm/types/denm_message.h"

#include <iostream>
#include <vector>

using namespace ns3;

namespace
{

PacketContext MakeContext(
    uint64_t packetId,
    std::initializer_list<uint8_t> payload)
{
    PacketContext ctx;
    ctx.packetId = packetId;
    ctx.timestamp = packetId * 1000;
    ctx.payload.bytes.assign(
        payload.begin(),
        payload.end());

    return ctx;
}

DenmMessage MakeMessage(
    DenmManagementAction action,
    uint32_t stationId,
    uint16_t sequence)
{
    DenmMessage message;
    message.action = action;
    message.actionId.originatingStationId = stationId;
    message.actionId.sequenceNumber = sequence;
    return message;
}

bool Expect(
    bool condition,
    const char* description)
{
    if (condition)
    {
        std::cout << "PASS: " << description << std::endl;
        return true;
    }

    std::cout << "FAIL: " << description << std::endl;
    return false;
}

} // namespace

int main()
{
    bool ok = true;

    DenmReplayContext replay;

    const DenmActionId actionId{
        42,
        1
    };

    // ------------------------------------------------------------
    // NEW
    // ------------------------------------------------------------

    PacketContext newCtx =
        MakeContext(1, {0x10, 0x11, 0x12});

    DenmMessage newMessage =
        MakeMessage(
            DenmManagementAction::NEW,
            actionId.originatingStationId,
            actionId.sequenceNumber);

    ok &= Expect(
        DenmReplayProtection::Process(
            newCtx,
            newMessage,
            replay),
        "first NEW is accepted");

    // ------------------------------------------------------------
    // UPDATE
    //
    // On the wire an UPDATE has the same ActionID and no
    // termination field. The decoder therefore initially represents
    // this as NEW. Replay/lifecycle state must classify it as UPDATE.
    // ------------------------------------------------------------

    PacketContext updateCtx =
        MakeContext(2, {0x20, 0x21, 0x22});

    DenmMessage updateMessage =
        MakeMessage(
            DenmManagementAction::NEW,
            actionId.originatingStationId,
            actionId.sequenceNumber);

    const bool updateAccepted =
        DenmReplayProtection::Process(
            updateCtx,
            updateMessage,
            replay);

    ok &= Expect(
        updateAccepted,
        "second packet with same ActionID and changed payload is accepted as UPDATE");

    ok &= Expect(
        replay.database.at(actionId).lastAction ==
            DenmManagementAction::UPDATE,
        "existing ActionID with changed payload is classified as UPDATE");

    // ------------------------------------------------------------
    // CANCELLATION
    // ------------------------------------------------------------

    PacketContext cancelCtx =
        MakeContext(3, {0x30, 0x31, 0x32});

    DenmMessage cancelMessage =
        MakeMessage(
            DenmManagementAction::CANCELLATION,
            actionId.originatingStationId,
            actionId.sequenceNumber);

    ok &= Expect(
        DenmReplayProtection::Process(
            cancelCtx,
            cancelMessage,
            replay),
        "CANCELLATION is accepted for the active ActionID");

    // ------------------------------------------------------------
    // UPDATE AFTER TERMINATION
    // ------------------------------------------------------------

    PacketContext afterCancelCtx =
        MakeContext(4, {0x40, 0x41, 0x42});

    DenmMessage afterCancelMessage =
        MakeMessage(
            DenmManagementAction::NEW,
            actionId.originatingStationId,
            actionId.sequenceNumber);

    ok &= Expect(
        !DenmReplayProtection::Process(
            afterCancelCtx,
            afterCancelMessage,
            replay),
        "UPDATE after CANCELLATION is rejected");

    // ------------------------------------------------------------
    // DUPLICATE UPDATE
    // ------------------------------------------------------------

    DenmReplayContext replay2;

    PacketContext firstCtx =
        MakeContext(5, {0x50, 0x51, 0x52});

    DenmMessage firstMessage =
        MakeMessage(
            DenmManagementAction::NEW,
            77,
            1);

    ok &= Expect(
        DenmReplayProtection::Process(
            firstCtx,
            firstMessage,
            replay2),
        "second lifecycle starts with NEW");

    PacketContext update1Ctx =
        MakeContext(6, {0x60, 0x61, 0x62});

    DenmMessage update1Message =
        MakeMessage(
            DenmManagementAction::NEW,
            77,
            1);

    ok &= Expect(
        DenmReplayProtection::Process(
            update1Ctx,
            update1Message,
            replay2),
        "first UPDATE for ActionID=77/1 is accepted");

    ok &= Expect(
        replay2.database.at(
            DenmActionId{77, 1}).lastAction ==
            DenmManagementAction::UPDATE,
        "ActionID=77/1 lifecycle state becomes UPDATE");

    PacketContext update2Ctx =
        MakeContext(7, {0x60, 0x61, 0x62});

    DenmMessage update2Message =
        MakeMessage(
            DenmManagementAction::NEW,
            77,
            1);

    ok &= Expect(
        !DenmReplayProtection::Process(
            update2Ctx,
            update2Message,
            replay2),
        "identical UPDATE payload is rejected as replay");

    if (!ok)
    {
        std::cout << "DENM_REPLAY_PROTECTION_EXIT=1" << std::endl;
        return 1;
    }

    std::cout << "DENM_REPLAY_PROTECTION_EXIT=0" << std::endl;
    return 0;
}
