#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
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

    auto context =
        std::make_shared<ns3::PacketContext>();
    context->senderVehId = "spatem-test";
    context->senderNodeId = 0;
    context->senderStationId = 1001;
    context->packetId = 1;

    const SpatemInput input =
        MakeDeterministicSpatem();

    bool transportCalled = false;
    ns3::BtpTxRequest capturedRequest{};
    std::vector<uint8_t> capturedPayload;

    ns3::SpatemTransportGateway gateway(
        [&](PacketContext& ctx,
            const BtpTxRequest& request,
            const std::vector<uint8_t>& payload)
        {
            transportCalled = true;
            capturedRequest = request;
            capturedPayload = payload;

            ctx.AdvancePipeline(
                PipelineStage::BTP_TX);

            ctx.MarkSuccess();

            ctx.AdvancePipeline(
                PipelineStage::GEONET_TX);

            ctx.MarkSuccess();

            return true;
        });

    const bool sent =
        ns3::SpatemService::Send(
            context,
            input,
            gateway,
            "deterministic_9_7");

    // Fake gateway is synchronous, therefore emulate the downstream
    // Access cleanup performed by real TX_COMPLETE.
    ns3::TxBridgeContext::Pop();

    ok &= Check(
        sent,
        "SpatemService accepted deterministic SPATEM");

    ok &= Check(
        transportCalled,
        "SPATEM transport gateway was invoked");

    ok &= Check(
        context->payload.type ==
            ns3::PayloadType::SPATEM,
        "TX PacketContext payload type is SPATEM");

    ok &= Check(
        capturedRequest.destinationPort == 2004,
        "BTP destination port is 2004");

    ok &= Check(
        capturedRequest.transport ==
            ns3::BtpTransportType::SHB,
        "SPATEM transport is SHB");

    ok &= Check(
        capturedRequest.communicationProfile ==
            ns3::BtpCommunicationProfile::ITS_G5,
        "SPATEM communication profile is ITS_G5");

    ok &= Check(
        capturedRequest.itsAid == 137,
        "SPATEM ITS-AID is 137");

    ok &= Check(
        !capturedPayload.empty(),
        "SPATEM UPER payload is non-empty");

    const auto decoded =
        VanetzaSpatemCodec::Decode(
            capturedPayload);

    ok &= Check(
        decoded.success,
        "transport payload decodes as SPATEM");

    if (decoded.success)
    {
        ok &= Check(
            decoded.message.stationId == 1001,
            "decoded station ID is 1001");

        ok &= Check(
            decoded.message.intersections.size() == 1,
            "decoded message contains one intersection");

        if (decoded.message.intersections.size() == 1)
        {
            const auto& intersection =
                decoded.message.intersections.front();

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
    }

    ok &= Check(
        context->pipelineStage ==
            ns3::PipelineStage::GEONET_TX,
        "PacketContext reaches GEONET_TX");

    ok &= Check(
        context->pipelineResult ==
            ns3::PipelineResult::OK,
        "PacketContext reports successful TX");

    std::cout
        << "SPATEM_TX_SERVICE_GREEN_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
