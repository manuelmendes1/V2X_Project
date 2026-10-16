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

bool BuildPayload(
    unsigned invalidTimingCase,
    std::vector<uint8_t>& payload)
{
    SpatemInput input;
    input.stationId = 9001;

    SpatemIntersection intersection;
    intersection.intersectionId = 42;
    intersection.revision = 7;
    intersection.minuteOfYear =
        invalidTimingCase == 0 ? 100U : 101U;
    intersection.dSecond =
        invalidTimingCase == 0 ? 1000U : 1001U;

    SpatemTiming timing;
    timing.minEndTime = 100;
    timing.maxEndTime = 200;
    timing.likelyTime = 150;

    SpatemMovementEvent event;
    event.phaseState =
        SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;
    event.timing = timing;

    SpatemMovement movement;
    movement.signalGroup = 1;
    movement.events.push_back(event);
    intersection.movements.push_back(movement);
    input.intersections.push_back(intersection);

    auto asn = SpatemAsn1Builder::Build(input);
    if (!asn)
    {
        std::cerr << "TEST_SETUP_FAILURE=asn1_builder\n";
        return false;
    }

    if (invalidTimingCase != 0)
    {
        auto* asnIntersection =
            asn->spat.intersections.list.array[0];
        auto* asnMovement =
            asnIntersection->states.list.array[0];
        auto* asnEvent =
            asnMovement->state_time_speed.list.array[0];

        if (asnEvent == nullptr || asnEvent->timing == nullptr ||
            asnEvent->timing->maxEndTime == nullptr ||
            asnEvent->timing->likelyTime == nullptr)
        {
            std::cerr << "TEST_SETUP_FAILURE=timing_field_missing\n";
            return false;
        }

        // Keep each timing value ASN.1-representable while violating
        // one semantic ordering rule at a time.
        if (invalidTimingCase == 1)
        {
            asnEvent->timing->minEndTime = 200;
            *asnEvent->timing->maxEndTime = 100;
        }
        else if (invalidTimingCase == 2)
        {
            *asnEvent->timing->likelyTime = 50;
        }
        else if (invalidTimingCase == 3)
        {
            *asnEvent->timing->likelyTime = 250;
        }
        else
        {
            std::cerr << "TEST_SETUP_FAILURE=unknown_timing_case\n";
            return false;
        }
    }

    const auto encoded = SpatemUperEncoder::Encode(*asn);
    if (!encoded.success || encoded.bytes.empty())
    {
        std::cerr
            << "TEST_SETUP_FAILURE=uper_encode"
            << " failed_type=" << encoded.failedType << '\n';
        return false;
    }

    payload = encoded.bytes;
    return true;
}

ns3::PacketContext MakeContext(uint64_t packetId)
{
    ns3::PacketContext context;
    context.packetId = packetId;
    context.timestamp = 123456 + packetId;
    context.btpDestinationPort = 2004;

    context.geonet.hasItsAid = true;
    context.geonet.itsAid = 137;

    context.senderStationId = 9001;
    context.senderNodeId = 5;
    context.receiverStationId = 9002;
    context.receiverNodeId = 6;

    context.payload.type = ns3::PayloadType::RAW;
    return context;
}

bool CheckValidTimingAccepted()
{
    std::vector<uint8_t> payload;
    if (!BuildPayload(0, payload))
    {
        return false;
    }

    auto context = MakeContext(501);
    SpatemMessage message{};
    ns3::SpatemReplayContext replay;

    const bool processed = ns3::SpatemRxPipeline::Process(
        context, payload, message, replay);

    const bool passed =
        processed &&
        message.intersections.size() == 1 &&
        message.intersections[0].movements.size() == 1 &&
        message.intersections[0].movements[0].events.size() == 1 &&
        message.intersections[0].movements[0].events[0].timing.has_value() &&
        message.intersections[0].movements[0].events[0].timing->minEndTime == 100 &&
        message.intersections[0].movements[0].events[0].timing->maxEndTime == 200 &&
        !replay.database.empty();

    std::cout
        << (passed ? "[PASS] " : "[FAIL] ")
        << "valid_timing_control"
        << " processed=" << processed
        << " reason=\"" << context.reason << "\""
        << " replay_entries=" << replay.database.size()
        << '\n';

    return passed;
}

bool CheckInvalidTimingRejectedBeforeReplay(
    unsigned invalidTimingCase,
    uint64_t packetId,
    const char* caseName)
{
    std::vector<uint8_t> payload;
    if (!BuildPayload(invalidTimingCase, payload))
    {
        return false;
    }

    auto context = MakeContext(packetId);
    SpatemMessage message{};
    ns3::SpatemReplayContext replay;

    const bool processed = ns3::SpatemRxPipeline::Process(
        context, payload, message, replay);

    const bool passed =
        !processed &&
        context.reason == "invalid_spatem_timing" &&
        context.pipelineStage == ns3::PipelineStage::DECODE &&
        replay.database.empty();

    std::cout
        << (passed ? "[PASS] " : "[FAIL] ")
        << caseName
        << " processed=" << processed
        << " stage=" << ns3::ToString(context.pipelineStage)
        << " reason=\"" << context.reason << "\""
        << " replay_entries=" << replay.database.size()
        << '\n';

    return passed;
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    if (CheckValidTimingAccepted())
        ++passed;
    else
        ++failed;

    const bool invalidCases[] = {
        CheckInvalidTimingRejectedBeforeReplay(
            1, 502, "min_end_after_max_end_rejected"),
        CheckInvalidTimingRejectedBeforeReplay(
            2, 503, "likely_time_before_minimum_rejected"),
        CheckInvalidTimingRejectedBeforeReplay(
            3, 504, "likely_time_after_maximum_rejected")
    };
    for (const bool result : invalidCases)
    {
        result ? ++passed : ++failed;
    }

    std::cout << "SPATEM_RX_SEMANTIC_TIMING_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_RX_SEMANTIC_TIMING_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_RX_SEMANTIC_TIMING_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_RX_SEMANTIC_TIMING_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
