#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/types/spatem_input.h"
#include "vanetza_integration/spatem/replay/spatem_replay_context.h"
#include "vanetza_integration/spatem/types/spatem_message.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"
#include "vanetza_integration/spatem/rx/spatem_rx_pipeline.h"

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
MakeDeterministicSpatem()
{
    SpatemInput input{};
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

    event.timing = timing;
    movement.events.push_back(event);
    intersection.movements.push_back(movement);
    input.intersections.push_back(intersection);

    return input;
}

} // namespace

int
main()
{
    bool ok = true;

    const auto input = MakeDeterministicSpatem();
    const auto encoded = VanetzaSpatemCodec::Encode(input);

    ok &= Check(
        encoded.success,
        "deterministic SPATEM encoded successfully");

    if (!encoded.success)
    {
        std::cerr
            << "SPATEM_RX_PIPELINE_GREEN_EXIT=1\n";
        return 1;
    }

    PacketContext context{};
    context.packetId = 9001;
    context.senderVehId = "veh0";
    context.senderNodeId = 0;
    context.senderStationId = 1001;
    context.receiverVehId = "veh1";
    context.receiverNodeId = 1;
    context.receiverStationId = 1002;

    context.btpDestinationPort = 2004;
    context.geonet.hasItsAid = true;
    context.geonet.itsAid = 137;

    SpatemMessage message{};
    ns3::SpatemReplayContext replay{};

    const bool delivered =
        SpatemRxPipeline::Process(
            context,
            encoded.bytes,
            message,
            replay);

    ok &= Check(
        delivered,
        "SPATEM RX pipeline accepted the payload");

    ok &= Check(
        context.pipelineStage ==
            PipelineStage::DELIVER,
        "PacketContext reaches DELIVER");

    ok &= Check(
        context.pipelineResult ==
            PipelineResult::OK,
        "PacketContext reports successful RX");

    ok &= Check(
        message.stationId == 1001,
        "decoded station ID is 1001");

    ok &= Check(
        message.intersections.size() == 1,
        "decoded message contains one intersection");

    if (message.intersections.size() == 1)
    {
        const auto& intersection =
            message.intersections.front();

        ok &= Check(
            intersection.intersectionId == 42,
            "decoded intersection ID is 42");

        ok &= Check(
            intersection.revision == 7,
            "decoded intersection revision is 7");

        ok &= Check(
            intersection.movements.size() == 1,
            "decoded intersection contains one movement");

        if (intersection.movements.size() == 1)
        {
            const auto& movement =
                intersection.movements.front();

            ok &= Check(
                movement.signalGroup == 3,
                "decoded signal group is 3");

            ok &= Check(
                movement.events.size() == 1,
                "decoded movement contains one event");

            if (movement.events.size() == 1)
            {
                const auto& event =
                    movement.events.front();

                ok &= Check(
                    event.phaseState ==
                        SpatemMovementPhaseState::
                            PROTECTED_MOVEMENT_ALLOWED,
                    "decoded phase state is PROTECTED_MOVEMENT_ALLOWED");

                ok &= Check(
                    event.timing.has_value(),
                    "decoded timing is present");

                if (event.timing)
                {
                    ok &= Check(
                        event.timing->minEndTime == 100,
                        "decoded minEndTime is 100");

                    ok &= Check(
                        event.timing->maxEndTime.has_value(),
                        "decoded maxEndTime is present");

                    if (event.timing->maxEndTime)
                    {
                        ok &= Check(
                            *event.timing->maxEndTime == 200,
                            "decoded maxEndTime is 200");
                    }
                }
            }
        }
    }

    // ============================================================
    // SPATEM-19 / 9.14 — wrong BTP destination port
    //
    // The payload is valid SPATEM, but the transport dispatch port
    // is intentionally wrong. The SPATEM pipeline must reject it at
    // BTP_RX, before VERIFY/DECODE/REPLAY/RECEPTION/DELIVER.
    //
    // This closes R-09 with explicit negative evidence.
    // ============================================================

    {
        PacketContext wrongPortContext{};
        wrongPortContext.packetId = 9004;

        wrongPortContext.senderVehId = "veh0";
        wrongPortContext.senderNodeId = 0;
        wrongPortContext.senderStationId = 1001;

        wrongPortContext.receiverVehId = "veh1";
        wrongPortContext.receiverNodeId = 1;
        wrongPortContext.receiverStationId = 1002;

        wrongPortContext.btpDestinationPort = 2005;
        wrongPortContext.geonet.hasItsAid = true;
        wrongPortContext.geonet.itsAid = 137;

        SpatemMessage wrongPortMessage{};
        SpatemReplayContext wrongPortReplay{};

        const bool wrongPortDelivered =
            SpatemRxPipeline::Process(
                wrongPortContext,
                encoded.bytes,
                wrongPortMessage,
                wrongPortReplay);

        ok &= Check(
            !wrongPortDelivered,
            "SPATEM with wrong BTP port is rejected");

        ok &= Check(
            wrongPortContext.pipelineResult ==
                PipelineResult::FAIL,
            "wrong BTP port context is marked FAIL");

        ok &= Check(
            wrongPortContext.reason ==
                "invalid_spatem_btp_port",
            "wrong BTP port reason is reported");

        ok &= Check(
            wrongPortContext.pipelineStage ==
                PipelineStage::BTP_RX,
            "wrong BTP port stops at BTP_RX");

        ok &= Check(
            wrongPortContext.pipelineStage !=
                PipelineStage::VERIFY,
            "wrong BTP port does not reach VERIFY");

        ok &= Check(
            wrongPortContext.pipelineStage !=
                PipelineStage::DECODE,
            "wrong BTP port does not reach DECODE");

        ok &= Check(
            wrongPortContext.pipelineStage !=
                PipelineStage::REPLAY,
            "wrong BTP port does not reach REPLAY");

        ok &= Check(
            wrongPortContext.pipelineStage !=
                PipelineStage::RECEPTION,
            "wrong BTP port does not reach RECEPTION");

        ok &= Check(
            wrongPortContext.pipelineStage !=
                PipelineStage::DELIVER,
            "wrong BTP port does not reach DELIVER");
    }

    // ============================================================
    // SPATEM-19 / 9.14 — sender identity mismatch
    //
    // The network/runtime resolved sender identity is station 2001,
    // while the SPATEM payload declares station 1001.
    //
    // This must be rejected before REPLAY because the application
    // payload identity does not match the resolved transport sender.
    //
    // Previously RED; now GREEN: SpatemRxPipeline compares
    // message.stationId with context.senderStationId.
    // ============================================================

    {
        PacketContext mismatchContext{};
        mismatchContext.packetId = 9003;

        mismatchContext.senderVehId = "veh2";
        mismatchContext.senderNodeId = 2;
        mismatchContext.senderStationId = 2001;

        mismatchContext.receiverVehId = "veh1";
        mismatchContext.receiverNodeId = 1;
        mismatchContext.receiverStationId = 1002;

        mismatchContext.btpDestinationPort = 2004;
        mismatchContext.geonet.hasItsAid = true;
        mismatchContext.geonet.itsAid = 137;

        SpatemMessage mismatchMessage{};
        SpatemReplayContext mismatchReplay{};

        const bool mismatchDelivered =
            SpatemRxPipeline::Process(
                mismatchContext,
                encoded.bytes,
                mismatchMessage,
                mismatchReplay);

        ok &= Check(
            !mismatchDelivered,
            "SPATEM sender identity mismatch is rejected");

        ok &= Check(
            mismatchContext.pipelineResult ==
                PipelineResult::DROP,
            "sender identity mismatch context is marked DROP");

        ok &= Check(
            mismatchContext.reason ==
                "spatem_sender_identity_mismatch",
            "sender identity mismatch reason is reported");

        ok &= Check(
            mismatchContext.pipelineStage !=
                PipelineStage::REPLAY,
            "sender identity mismatch does not reach REPLAY");

        ok &= Check(
            mismatchContext.pipelineStage !=
                PipelineStage::RECEPTION,
            "sender identity mismatch does not reach RECEPTION");

        ok &= Check(
            mismatchContext.pipelineStage !=
                PipelineStage::DELIVER,
            "sender identity mismatch does not reach DELIVER");
    }

    // ============================================================
    // SPATEM-19 / 9.14 — self-origin
    //
    // The sender and receiver are the same station. A SPATEM
    // originating from that station must be rejected before
    // REPLAY/RECEPTION/DELIVER.
    //
    // Previously RED; now GREEN: SpatemRxPipeline enforces self-origin rejection.
    // ============================================================

    {
        PacketContext selfContext{};
        selfContext.packetId = 9002;

        selfContext.senderVehId = "veh1";
        selfContext.senderNodeId = 1;
        selfContext.senderStationId = 1001;

        selfContext.receiverVehId = "veh1";
        selfContext.receiverNodeId = 1;
        selfContext.receiverStationId = 1001;

        selfContext.btpDestinationPort = 2004;
        selfContext.geonet.hasItsAid = true;
        selfContext.geonet.itsAid = 137;

        SpatemMessage selfMessage{};
        SpatemReplayContext selfReplay{};

        const bool selfDelivered =
            SpatemRxPipeline::Process(
                selfContext,
                encoded.bytes,
                selfMessage,
                selfReplay);

        ok &= Check(
            !selfDelivered,
            "self-originated SPATEM is rejected");

        ok &= Check(
            selfContext.pipelineResult ==
                PipelineResult::DROP,
            "self-originated SPATEM context is marked DROP");

        ok &= Check(
            selfContext.reason == "self_reception",
            "self-originated SPATEM reason is self_reception");

        ok &= Check(
            selfContext.pipelineStage !=
                PipelineStage::RECEPTION,
            "self-originated SPATEM does not reach RECEPTION");

        ok &= Check(
            selfContext.pipelineStage !=
                PipelineStage::DELIVER,
            "self-originated SPATEM does not reach DELIVER");
    }

    std::cout
        << "SPATEM_RX_PIPELINE_GREEN_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
