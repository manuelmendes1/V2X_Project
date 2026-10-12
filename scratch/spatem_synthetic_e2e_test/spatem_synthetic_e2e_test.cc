#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ns3/simulator.h"

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/spatem/replay/spatem_replay_context.h"
#include "vanetza_integration/spatem/rx/spatem_rx_pipeline.h"
#include "vanetza_integration/spatem/service/spatem_service.h"
#include "vanetza_integration/spatem/transport/spatem_transport_gateway.h"
#include "vanetza_integration/spatem/types/spatem_input.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

using namespace ns3;

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

SpatemInput
MakeSyntheticSpatem()
{
    SpatemInput input{};

    // Infrastructure station identity at application level.
    input.stationId = 1001;

    SpatemIntersection intersection{};
    intersection.intersectionId = 42;
    intersection.revision = 7;
    intersection.statusBits = 0;

    SpatemMovement movement{};
    movement.signalGroup = 3;

    SpatemMovementEvent event{};
    event.phaseState =
        SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;

    SpatemTiming timing{};
    timing.startTime = 50;
    timing.minEndTime = 100;
    timing.maxEndTime = 200;
    timing.likelyTime = 150;
    timing.confidence = 7;
    timing.nextTime = 250;

    event.timing = timing;

    movement.events.push_back(event);
    intersection.movements.push_back(movement);
    input.intersections.push_back(intersection);

    return input;
}

bool
EqualSemanticContent(
    const SpatemInput& input,
    const SpatemMessage& output)
{
    if (input.stationId != output.stationId)
    {
        return false;
    }

    if (input.intersections.size() != output.intersections.size())
    {
        return false;
    }

    const auto& lhsIntersection =
        input.intersections.front();
    const auto& rhsIntersection =
        output.intersections.front();

    if (lhsIntersection.intersectionId !=
        rhsIntersection.intersectionId)
    {
        return false;
    }

    if (lhsIntersection.revision !=
        rhsIntersection.revision)
    {
        return false;
    }

    if (lhsIntersection.movements.size() !=
        rhsIntersection.movements.size())
    {
        return false;
    }

    const auto& lhsMovement =
        lhsIntersection.movements.front();
    const auto& rhsMovement =
        rhsIntersection.movements.front();

    if (lhsMovement.signalGroup !=
        rhsMovement.signalGroup)
    {
        return false;
    }

    if (lhsMovement.events.size() !=
        rhsMovement.events.size())
    {
        return false;
    }

    const auto& lhsEvent =
        lhsMovement.events.front();
    const auto& rhsEvent =
        rhsMovement.events.front();

    if (lhsEvent.phaseState !=
        rhsEvent.phaseState)
    {
        return false;
    }

    if (lhsEvent.timing.has_value() !=
        rhsEvent.timing.has_value())
    {
        return false;
    }

    if (!lhsEvent.timing)
    {
        return true;
    }

    const auto& lhsTiming = *lhsEvent.timing;
    const auto& rhsTiming = *rhsEvent.timing;

    return
        lhsTiming.startTime == rhsTiming.startTime &&
        lhsTiming.minEndTime == rhsTiming.minEndTime &&
        lhsTiming.maxEndTime == rhsTiming.maxEndTime &&
        lhsTiming.likelyTime == rhsTiming.likelyTime &&
        lhsTiming.confidence == rhsTiming.confidence &&
        lhsTiming.nextTime == rhsTiming.nextTime;
}

} // namespace

int
main()
{
    bool ok = true;

    std::cout
        << "============================================================\n"
        << "ETAPA 9.12 — SYNTHETIC SPATEM E2E\n"
        << "============================================================\n";

    const SpatemInput input =
        MakeSyntheticSpatem();

    /*
     * -----------------------------------------------------------------------
     * TX: application model -> SPATEM service -> in-memory transport
     *
     * No socket, Wi-Fi, channel or SUMO dependency exists in this path.
     * -----------------------------------------------------------------------
     */

    auto txContext =
        std::make_shared<PacketContext>();

    txContext->senderVehId = "infra0";
    txContext->senderNodeId = 100;
    txContext->senderStationId = input.stationId;
    txContext->packetId = 9201;

    std::vector<uint8_t> capturedPayload;
    BtpTxRequest capturedRequest{};
    bool transportCalled = false;

    SpatemTransportGateway gateway(
        [&](PacketContext& context,
            const BtpTxRequest& request,
            const std::vector<uint8_t>& payload)
        {
            transportCalled = true;
            capturedRequest = request;
            capturedPayload = payload;

            context.AdvancePipeline(
                PipelineStage::BTP_TX);
            context.MarkSuccess();

            context.AdvancePipeline(
                PipelineStage::GEONET_TX);
            context.MarkSuccess();

            context.AdvancePipeline(
                PipelineStage::ACCESS_TX);
            context.MarkSuccess();

            context.AdvancePipeline(
                PipelineStage::SEND);
            context.MarkSuccess();

            context.AdvancePipeline(
                PipelineStage::TX_COMPLETE);
            context.MarkSuccess();

            return true;
        });

    const bool sent =
        SpatemService::Send(
            txContext,
            input,
            gateway,
            "synthetic_9_12");

    TxBridgeContext::Pop();

    ok &= Check(
        sent,
        "synthetic SPATEM TX service accepted input");

    ok &= Check(
        transportCalled,
        "in-memory transport gateway was invoked");

    ok &= Check(
        capturedRequest.destinationPort == 2004,
        "synthetic BTP destination port is 2004");

    ok &= Check(
        capturedRequest.transport ==
            BtpTransportType::SHB,
        "synthetic transport is SHB");

    ok &= Check(
        capturedRequest.communicationProfile ==
            BtpCommunicationProfile::ITS_G5,
        "synthetic communication profile is ITS_G5");

    ok &= Check(
        capturedRequest.itsAid == 137,
        "synthetic ITS-AID is 137");

    ok &= Check(
        !capturedPayload.empty(),
        "TX produced non-empty SPATEM payload");

    ok &= Check(
        txContext->payload.type ==
            PayloadType::SPATEM,
        "TX PacketContext payload type is SPATEM");

    ok &= Check(
        txContext->pipelineStage ==
            PipelineStage::TX_COMPLETE,
        "TX PacketContext reaches TX_COMPLETE");

    ok &= Check(
        txContext->pipelineResult ==
            PipelineResult::OK,
        "TX PacketContext reports successful completion");

    /*
     * -----------------------------------------------------------------------
     * Independent wire round-trip before RX pipeline.
     * -----------------------------------------------------------------------
     */

    const auto decodedWire =
        VanetzaSpatemCodec::Decode(
            capturedPayload);

    ok &= Check(
        decodedWire.success,
        "captured in-memory wire payload decodes");

    if (decodedWire.success)
    {
        ok &= Check(
            EqualSemanticContent(
                input,
                decodedWire.message),
            "TX semantic SPATEM content survives encode/decode");
    }

    /*
     * -----------------------------------------------------------------------
     * RX: captured TX bytes -> real SPATEM RX pipeline
     *
     * This is deliberately a different PacketContext, representing
     * infrastructure -> vehicle reception.
     * -----------------------------------------------------------------------
     */

    PacketContext rxContext{};

    rxContext.packetId = 9202;
    rxContext.receiverVehId = "veh1";
    rxContext.receiverNodeId = 1;
    rxContext.receiverStationId = 1002;

    rxContext.btpDestinationPort = 2004;
    rxContext.geonet.hasItsAid = true;
    rxContext.geonet.itsAid = 137;

    rxContext.payload.bytes = capturedPayload;

    SpatemMessage received{};
    SpatemReplayContext replay{};

    const bool delivered =
        SpatemRxPipeline::Process(
            rxContext,
            capturedPayload,
            received,
            replay);

    ok &= Check(
        delivered,
        "synthetic SPATEM RX pipeline accepted TX payload");

    ok &= Check(
        rxContext.pipelineStage ==
            PipelineStage::DELIVER,
        "RX PacketContext reaches DELIVER");

    ok &= Check(
        rxContext.pipelineResult ==
            PipelineResult::OK,
        "RX PacketContext reports successful delivery");

    ok &= Check(
        EqualSemanticContent(
            input,
            received),
        "TX and RX semantic SPATEM content are identical");

    ok &= Check(
        received.stationId == 1001,
        "origin station ID survives E2E");

    ok &= Check(
        received.intersections.size() == 1 &&
        received.intersections.front().intersectionId == 42,
        "intersection ID survives E2E");

    ok &= Check(
        received.intersections.front().revision == 7,
        "intersection revision survives E2E");

    ok &= Check(
        received.intersections.front().movements.size() == 1 &&
        received.intersections.front().movements.front().signalGroup == 3,
        "signal group survives E2E");

    ok &= Check(
        received.intersections.front()
            .movements.front()
            .events.front()
            .phaseState ==
        SpatemMovementPhaseState::
            PROTECTED_MOVEMENT_ALLOWED,
        "phase state survives E2E");

    const auto& receivedTiming =
        received.intersections.front()
            .movements.front()
            .events.front()
            .timing;

    ok &= Check(
        receivedTiming.has_value(),
        "SPATEM timing survives E2E");

    if (receivedTiming)
    {
        ok &= Check(
            receivedTiming->minEndTime == 100,
            "minEndTime survives E2E");

        ok &= Check(
            receivedTiming->maxEndTime.has_value() &&
            *receivedTiming->maxEndTime == 200,
            "maxEndTime survives E2E");

        ok &= Check(
            receivedTiming->likelyTime.has_value() &&
            *receivedTiming->likelyTime == 150,
            "likelyTime survives E2E");

        ok &= Check(
            receivedTiming->confidence.has_value() &&
            *receivedTiming->confidence == 7,
            "confidence survives E2E");

        ok &= Check(
            receivedTiming->nextTime.has_value() &&
            *receivedTiming->nextTime == 250,
            "nextTime survives E2E");
    }

    /*
     * The current SPATEM VERIFY stage is bounded transport/AID
     * verification, not cryptographic signature verification.
     */
    ok &= Check(
        rxContext.btpDestinationPort == 2004 &&
        rxContext.geonet.hasItsAid &&
        rxContext.geonet.itsAid == 137,
        "RX transport/security identity is valid for SPATEM");

    std::cout
        << "SYNTHETIC_E2E_SOCKET_DEPENDENCIES=0\n"
        << "SYNTHETIC_E2E_PAYLOAD_CAPTURED="
        << (!capturedPayload.empty() ? "YES" : "NO")
        << "\n"
        << "SYNTHETIC_E2E_SEMANTICS="
        << (EqualSemanticContent(input, received) ? "MATCH" : "MISMATCH")
        << "\n"
        << "SPATEM_SYNTHETIC_E2E_GREEN_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
