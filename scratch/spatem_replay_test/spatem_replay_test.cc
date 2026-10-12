#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/types/spatem_message.h"
#include "vanetza_integration/spatem/types/spatem_input.h"
#include "vanetza_integration/spatem/replay/spatem_replay_context.h"
#include "vanetza_integration/spatem/replay/spatem_replay_protection.h"

namespace
{

bool
Check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << "\n";
        return false;
    }

    std::cout << "PASS " << message << "\n";
    return true;
}

SpatemMessage
MakeMessage(uint16_t dSecond)
{
    SpatemMessage message{};

    message.stationId = 1001;

    SpatemIntersection intersection{};
    intersection.intersectionId = 42;
    intersection.revision = 7;
    intersection.statusBits = 0;
    intersection.minuteOfYear = 1000;
    intersection.dSecond = dSecond;

    SpatemMovement movement{};
    movement.signalGroup = 3;

    SpatemMovementEvent event{};
    event.phaseState =
        SpatemMovementPhaseState::
            PROTECTED_MOVEMENT_ALLOWED;

    SpatemTiming timing{};
    timing.minEndTime = 100;
    timing.maxEndTime = 200;
    event.timing = timing;

    movement.events.push_back(event);
    intersection.movements.push_back(movement);
    message.intersections.push_back(intersection);

    return message;
}

ns3::PacketContext
MakeContext(const std::vector<uint8_t>& payload,
            uint64_t packetId)
{
    ns3::PacketContext context{};

    context.packetId = packetId;
    context.senderVehId = "veh0";
    context.senderStationId = 1001;
    context.receiverVehId = "veh1";
    context.receiverStationId = 1002;
    context.btpDestinationPort = 2004;

    context.geonet.hasItsAid = true;
    context.geonet.itsAid = 137;

    context.payload.bytes = payload;

    return context;
}

} // namespace

int
main()
{
    bool ok = true;

    std::cout
        << "============================================================\n"
        << "ETAPA 9.9 — SPATEM REPLAY GREEN TEST\n"
        << "============================================================\n";

    ns3::SpatemReplayContext replay{};

    const std::vector<uint8_t> payloadA{
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06
    };

    const auto messageA =
        MakeMessage(1000);

    auto contextA =
        MakeContext(payloadA, 9101);

    const bool first =
        ns3::SpatemReplayProtection::Process(
            contextA,
            messageA,
            replay);

    ok &= Check(
        first,
        "first SPATEM message is accepted");

    const auto messageReplay =
        MakeMessage(1000);

    auto contextReplay =
        MakeContext(payloadA, 9102);

    const bool replayed =
        ns3::SpatemReplayProtection::Process(
            contextReplay,
            messageReplay,
            replay);

    ok &= Check(
        !replayed,
        "identical SPATEM message is rejected as replay");

    const auto messageNewTime =
        MakeMessage(1100);

    auto contextNewTime =
        MakeContext(payloadA, 9103);

    const bool newTime =
        ns3::SpatemReplayProtection::Process(
            contextNewTime,
            messageNewTime,
            replay);

    ok &= Check(
        newTime,
        "same payload with newer SPATEM time is accepted");

    const std::vector<uint8_t> payloadB{
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x07
    };

    const auto messageSameTimeDifferentPayload =
        MakeMessage(1100);

    auto contextDifferentPayload =
        MakeContext(payloadB, 9104);

    const bool differentPayload =
        ns3::SpatemReplayProtection::Process(
            contextDifferentPayload,
            messageSameTimeDifferentPayload,
            replay);

    ok &= Check(
        differentPayload,
        "different SPATEM payload with same timestamp is accepted");

    const auto messageOlderTime =
        MakeMessage(900);

    auto contextOlderTime =
        MakeContext(payloadA, 9105);

    const bool olderTime =
        ns3::SpatemReplayProtection::Process(
            contextOlderTime,
            messageOlderTime,
            replay);

    ok &= Check(
        !olderTime,
        "older SPATEM time is rejected");

    ok &= Check(
        contextOlderTime.pipelineResult ==
            ns3::PipelineResult::DROP,
        "older timestamp context is marked DROP");

    ok &= Check(
        contextReplay.pipelineResult ==
            ns3::PipelineResult::DROP,
        "replayed context is marked DROP");

    ok &= Check(
        contextNewTime.pipelineResult ==
            ns3::PipelineResult::OK,
        "new timestamp context is marked OK");

    std::cout
        << "SPATEM_REPLAY_RED_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
