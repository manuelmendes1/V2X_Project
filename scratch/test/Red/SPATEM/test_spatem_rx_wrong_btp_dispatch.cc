#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/spatem/rx/spatem_rx_pipeline.h"
#include "vanetza_integration/spatem/replay/spatem_replay_context.h"
#include "vanetza_integration/wrappers/vanetza/spatem/spatem_asn1_builder.h"
#include "vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_encoder.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

// Suppress log output in this isolated unit test. Pipeline behavior and
// PacketContext state remain the production implementations.
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

SpatemInput ValidInput()
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

    return input;
}

bool CheckWrongBtpPortRejected()
{
    const auto asn = SpatemAsn1Builder::Build(ValidInput());

    if (!asn)
    {
        std::cout << "[FAIL] wrong_btp_port_rejected"
                  << " reason=builder_failed\n";
        return false;
    }

    const auto encoded = SpatemUperEncoder::Encode(*asn);

    if (!encoded.success)
    {
        std::cout << "[FAIL] wrong_btp_port_rejected"
                  << " reason=payload_encoding_failed"
                  << " error=\"" << encoded.failedType << "\"\n";
        return false;
    }

    // Ensure that the input payload is valid before testing BTP dispatch.
    const auto codecControl =
        VanetzaSpatemCodec::Decode(encoded.bytes);

    if (!codecControl.success)
    {
        std::cout << "[FAIL] wrong_btp_port_rejected"
                  << " reason=valid_payload_control_failed"
                  << " error=\"" << codecControl.error << "\"\n";
        return false;
    }

    ns3::PacketContext context;
    context.btpDestinationPort = 2001; // Deliberately not SPATEM port 2004.
    context.geonet.hasItsAid = true;
    context.geonet.itsAid = 137;

    context.senderStationId = 9001;
    context.senderNodeId = 1;
    context.receiverStationId = 9002;
    context.receiverNodeId = 2;
    context.timestamp = 123456;

    // Sentinels prove that dispatch rejection happens before payload binding
    // and does not overwrite the caller's message or replay state.
    context.payload.type = ns3::PayloadType::RAW;
    context.payload.bytes = {0xDE, 0xAD, 0xBE, 0xEF};

    const std::vector<uint8_t> originalContextPayload =
        context.payload.bytes;

    SpatemMessage message;
    message.stationId = 7777;

    ns3::SpatemReplayContext replay;

    const bool delivered = ns3::SpatemRxPipeline::Process(
        context,
        encoded.bytes,
        message,
        replay);

    const bool passed =
        !delivered &&
        context.pipelineStage == ns3::PipelineStage::BTP_RX &&
        context.pipelineResult == ns3::PipelineResult::FAIL &&
        context.reason == "invalid_spatem_btp_port" &&
        context.payload.bytes == originalContextPayload &&
        context.payload.type == ns3::PayloadType::RAW &&
        message.stationId == 7777 &&
        message.intersections.empty() &&
        replay.database.empty();

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << "wrong_btp_port_rejected"
              << " delivered=" << delivered
              << " stage=" << ns3::ToString(context.pipelineStage)
              << " result=" << ns3::ToString(context.pipelineResult)
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

    if (CheckWrongBtpPortRejected())
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    std::cout << "SPATEM_WRONG_BTP_DISPATCH_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_WRONG_BTP_DISPATCH_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_WRONG_BTP_DISPATCH_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_WRONG_BTP_DISPATCH_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
