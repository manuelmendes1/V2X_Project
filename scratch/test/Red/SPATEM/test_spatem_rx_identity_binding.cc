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

bool BuildPayload(std::vector<uint8_t>& payload)
{
    SpatemInput input;
    input.stationId = 9001;

    SpatemIntersection intersection;
    intersection.intersectionId = 42;

    SpatemMovement movement;
    movement.signalGroup = 1;

    SpatemMovementEvent event;
    event.phaseState =
        SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;

    movement.events.push_back(event);
    intersection.movements.push_back(movement);
    input.intersections.push_back(intersection);

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

ns3::PacketContext MakeContext(
    uint32_t senderStation,
    uint32_t senderNode,
    uint32_t receiverStation,
    uint32_t receiverNode,
    uint64_t packetId)
{
    ns3::PacketContext context;
    context.packetId = packetId;
    context.timestamp = 123456 + packetId;

    context.btpDestinationPort = 2004;
    context.geonet.hasItsAid = true;
    context.geonet.itsAid = 137;

    context.senderStationId = senderStation;
    context.senderNodeId = senderNode;
    context.receiverStationId = receiverStation;
    context.receiverNodeId = receiverNode;

    context.payload.type = ns3::PayloadType::RAW;
    context.payload.bytes = {0xDE, 0xAD, 0xBE, 0xEF};

    return context;
}

bool Report(
    const char* name,
    bool passed,
    const ns3::PacketContext& context,
    bool processed,
    const ns3::SpatemReplayContext& replay)
{
    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << name
              << " processed=" << processed
              << " reason=\"" << context.reason << "\""
              << " replay_entries=" << replay.database.size()
              << '\n';

    return passed;
}

bool CheckMissingSenderIdentity(const std::vector<uint8_t>& payload)
{
    auto context = MakeContext(0, 5, 9002, 6, 101);
    SpatemMessage message{};
    ns3::SpatemReplayContext replay;

    const bool processed = ns3::SpatemRxPipeline::Process(
        context, payload, message, replay);

    const bool passed =
        !processed &&
        context.reason == "spatem_sender_identity_missing" &&
        replay.database.empty();

    return Report(
        "missing_sender_identity_rejected",
        passed, context, processed, replay);
}

bool CheckWrongSenderIdentity(const std::vector<uint8_t>& payload)
{
    auto context = MakeContext(9002, 5, 9003, 6, 102);
    SpatemMessage message{};
    ns3::SpatemReplayContext replay;

    const bool processed = ns3::SpatemRxPipeline::Process(
        context, payload, message, replay);

    const bool passed =
        !processed &&
        context.reason == "spatem_sender_identity_mismatch" &&
        replay.database.empty();

    return Report(
        "wrong_sender_identity_rejected",
        passed, context, processed, replay);
}

bool CheckSelfOrigin(const std::vector<uint8_t>& payload)
{
    auto context = MakeContext(9001, 5, 9001, 5, 103);
    SpatemMessage message{};
    ns3::SpatemReplayContext replay;

    const bool processed = ns3::SpatemRxPipeline::Process(
        context, payload, message, replay);

    const bool passed =
        !processed &&
        context.reason == "self_reception" &&
        replay.database.empty();

    return Report(
        "self_origin_dropped_before_replay",
        passed, context, processed, replay);
}

bool CheckRemoteSenderDelivery(const std::vector<uint8_t>& payload)
{
    auto context = MakeContext(9001, 5, 9002, 6, 104);
    SpatemMessage message{};
    ns3::SpatemReplayContext replay;

    const bool processed = ns3::SpatemRxPipeline::Process(
        context, payload, message, replay);

    const bool passed =
        processed &&
        context.payload.type == ns3::PayloadType::SPATEM &&
        message.stationId == 9001 &&
        !replay.database.empty();

    return Report(
        "valid_remote_sender_accepted",
        passed, context, processed, replay);
}

} // namespace

int main()
{
    std::vector<uint8_t> payload;
    if (!BuildPayload(payload))
    {
        return 2;
    }

    unsigned passed = 0;
    unsigned failed = 0;

    const bool results[] = {
        CheckMissingSenderIdentity(payload),
        CheckWrongSenderIdentity(payload),
        CheckSelfOrigin(payload),
        CheckRemoteSenderDelivery(payload)
    };

    for (const bool result : results)
    {
        result ? ++passed : ++failed;
    }

    std::cout << "SPATEM_RX_IDENTITY_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_RX_IDENTITY_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_RX_IDENTITY_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_RX_IDENTITY_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
