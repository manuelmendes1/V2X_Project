#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/spatem/rx/spatem_rx_pipeline.h"
#include "vanetza_integration/spatem/replay/spatem_replay_context.h"
#include "vanetza_integration/wrappers/vanetza/spatem/spatem_asn1_builder.h"
#include "vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_encoder.h"

void V2XWriteLog(
    const ns3::PacketContext&,
    V2XLogLevel,
    const std::string&,
    const std::string&,
    const std::string&)
{
}

namespace
{

SpatemIntersection MakeIntersection(
    uint16_t intersectionId,
    uint8_t revision,
    uint8_t signalGroup,
    uint32_t dSecond)
{
    SpatemIntersection intersection;
    intersection.intersectionId = intersectionId;
    intersection.revision = revision;
    intersection.minuteOfYear = 100U;
    intersection.dSecond = dSecond;

    SpatemMovement movement;
    movement.signalGroup = signalGroup;

    SpatemMovementEvent event;
    event.phaseState =
        SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;

    movement.events.push_back(event);
    intersection.movements.push_back(movement);

    return intersection;
}

bool BuildPayload(std::vector<uint8_t>& payload)
{
    SpatemInput input;
    input.stationId = 9001;
    input.intersections.push_back(
        MakeIntersection(42, 7, 1, 1000));
    input.intersections.push_back(
        MakeIntersection(43, 8, 2, 2000));

    const auto asn = SpatemAsn1Builder::Build(input);
    if (!asn)
    {
        std::cerr << "TEST_SETUP_FAILURE=asn1_builder\n";
        return false;
    }

    const auto encoded = SpatemUperEncoder::Encode(*asn);
    if (!encoded.success)
    {
        std::cerr << "TEST_SETUP_FAILURE=uper_encoder "
                  << encoded.failedType << '\n';
        return false;
    }

    payload = encoded.bytes;
    return true;
}

ns3::PacketContext MakeContext()
{
    ns3::PacketContext context;
    context.packetId = 901;
    context.timestamp = 1234567;

    context.btpDestinationPort = 2004;
    context.geonet.hasItsAid = true;
    context.geonet.itsAid = 137;

    context.senderStationId = 9001;
    context.senderNodeId = 5;
    context.receiverStationId = 9002;
    context.receiverNodeId = 6;

    context.payload.type = ns3::PayloadType::RAW;
    context.payload.bytes = {0xDE, 0xAD, 0xBE, 0xEF};

    return context;
}

bool HasExpectedIntersection(
    const SpatemIntersection& actual,
    uint16_t intersectionId,
    uint8_t revision,
    uint8_t signalGroup)
{
    return actual.intersectionId == intersectionId &&
           actual.revision == revision &&
           actual.movements.size() == 1U &&
           actual.movements[0].signalGroup == signalGroup &&
           actual.movements[0].events.size() == 1U &&
           actual.movements[0].events[0].phaseState ==
               SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;
}

bool CheckRxMultiIntersection(
    const std::vector<uint8_t>& payload)
{
    auto context = MakeContext();
    SpatemMessage message{};
    ns3::SpatemReplayContext replay;

    const bool processed = ns3::SpatemRxPipeline::Process(
        context, payload, message, replay);

    bool replayHas42 = false;
    bool replayHas43 = false;

    for (const auto& item : replay.database)
    {
        replayHas42 = replayHas42 ||
            item.first.intersectionId == 42;
        replayHas43 = replayHas43 ||
            item.first.intersectionId == 43;
    }

    const bool intersectionsPreserved =
        message.intersections.size() == 2U &&
        HasExpectedIntersection(message.intersections[0], 42, 7, 1) &&
        HasExpectedIntersection(message.intersections[1], 43, 8, 2);

    const bool passed =
        processed &&
        context.reason.empty() &&
        context.payload.type == ns3::PayloadType::SPATEM &&
        message.stationId == 9001 &&
        intersectionsPreserved &&
        replay.database.size() == 2U &&
        replayHas42 &&
        replayHas43;

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << "rx_multi_intersection_delivery"
              << " processed=" << processed
              << " station_id=" << message.stationId
              << " intersection_count=" << message.intersections.size()
              << " intersection_ids=";

    if (message.intersections.empty())
    {
        std::cout << "none";
    }
    else
    {
        for (std::size_t i = 0; i < message.intersections.size(); ++i)
        {
            if (i != 0)
            {
                std::cout << ',';
            }
            std::cout << message.intersections[i].intersectionId;
        }
    }

    std::cout << " replay_entries=" << replay.database.size()
              << " replay_has_42=" << replayHas42
              << " replay_has_43=" << replayHas43
              << " reason=\"" << context.reason << "\"\n";

    return passed;
}

} // namespace

int main()
{
    std::vector<uint8_t> payload;
    if (!BuildPayload(payload))
    {
        return 2;
    }

    const bool passed = CheckRxMultiIntersection(payload);

    std::cout << "SPATEM_RX_MULTI_INTERSECTION_CASES=1\n";
    std::cout << "SPATEM_RX_MULTI_INTERSECTION_PASSED="
              << (passed ? 1 : 0) << '\n';
    std::cout << "SPATEM_RX_MULTI_INTERSECTION_FAILED="
              << (passed ? 0 : 1) << '\n';
    std::cout << "SPATEM_RX_MULTI_INTERSECTION_TESTS="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
