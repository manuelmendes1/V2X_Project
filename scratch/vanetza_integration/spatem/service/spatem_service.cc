#include "spatem_service.h"

#include "ns3/simulator.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

namespace ns3
{

bool
SpatemService::Send(const std::shared_ptr<PacketContext>& context,
                    const SpatemInput& input,
                    const SpatemTransportGateway& gateway,
                    const std::string& reason)
{
    if (!context)
    {
        return false;
    }

    PacketContext& ctx = *context;

    const auto now = Simulator::Now();
    ctx.simulationStep = SimulationContext::simulationStep;
    ctx.simulationTime = now.GetSeconds();
    ctx.timestamp = now.GetMicroSeconds();

    ctx.AdvancePipeline(PipelineStage::GENERATE);

    if (!input.stationId || input.intersections.empty())
    {
        ctx.MarkFailure();
        ctx.reason = "invalid_spatem_input";
        return false;
    }

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "SPATEM",
        "SPATEM_GENERATE_OK",
        "station=" + std::to_string(input.stationId)
            + " intersections=" + std::to_string(input.intersections.size()));

    ctx.AdvancePipeline(PipelineStage::ENCODE);

    const auto encoded = VanetzaSpatemCodec::Encode(input);

    if (!encoded.success || encoded.bytes.empty())
    {
        ctx.MarkFailure();
        ctx.reason = encoded.error.empty()
            ? "spatem_encode_failed"
            : encoded.error;
        return false;
    }

    ctx.payload.bytes = encoded.bytes;
    ctx.payload.type = PayloadType::SPATEM;
    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "SPATEM",
        "SPATEM_ENCODE_OK",
        "bytes=" + std::to_string(encoded.bytes.size())
            + " encoded_bits=" + std::to_string(encoded.encodedBits));

    if (!reason.empty())
    {
        ctx.reason = reason;
    }

    // Preserve the shared PacketContext across the asynchronous
    // BTP -> GeoNetworking -> Access path until TX_COMPLETE.
    TxBridgeContext::Push(context);

    const bool sent =
        gateway.Send(ctx, encoded.bytes);

    if (!sent)
    {
        ctx.MarkFailure();

        if (ctx.reason.empty())
        {
            ctx.reason = "spatem_transport_send_failed";
        }

        TxBridgeContext::Pop();
        return false;
    }

    // On success, the Access layer owns the eventual Pop() after
    // SEND / TX_COMPLETE. Do not pop here.
    return true;
}

} // namespace ns3
