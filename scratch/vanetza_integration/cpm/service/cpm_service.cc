#include "vanetza_integration/cpm/service/cpm_service.h"

#include <utility>

#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"

namespace ns3
{

bool
CpmService::Send(
    const std::shared_ptr<PacketContext>& context,
    const CpmInput& input,
    const CpmTransportGateway& gateway,
    uint8_t trafficClass)
{
    if (!context)
    {
        return false;
    }

    PacketContext& ctx = *context;

    ctx.payload.Clear();
    ctx.AdvancePipeline(PipelineStage::GENERATE);

    if (input.station.stationId == 0 ||
        ctx.senderStationId == 0 ||
        input.station.stationId != ctx.senderStationId)
    {
        ctx.MarkFailure();
        ctx.reason = "cpm_sender_station_id_mismatch";
        return false;
    }

    ctx.MarkSuccess();
    ctx.AdvancePipeline(PipelineStage::ENCODE);

    const CpmUperEncodeResult encoded =
        CpmUperEncoder::Encode(input);

    if (!encoded.success ||
        encoded.bytes.empty() ||
        encoded.encodedBits == 0 ||
        encoded.encodedBits > encoded.bytes.size() * 8 ||
        encoded.encodedBits <= (encoded.bytes.size() - 1) * 8)
    {
        ctx.MarkFailure();
        ctx.reason = encoded.error.empty()
            ? "cpm_uper_encode_failed"
            : encoded.error;
        ctx.payload.Clear();
        return false;
    }

    ctx.payload.bytes = encoded.bytes;
    ctx.payload.type = PayloadType::CPM;
    ctx.MarkSuccess();

    // The access layer owns the successful asynchronous TX lifecycle.
    // On failure, this service removes the context it pushed.
    TxBridgeContext::Push(context);
    ctx.AdvancePipeline(PipelineStage::BTP_TX);

    const bool sent =
        gateway.Send(ctx, encoded.bytes, trafficClass);

    if (!sent)
    {
        ctx.MarkFailure();

        if (ctx.reason.empty())
        {
            ctx.reason = "cpm_transport_send_failed";
        }

        TxBridgeContext::Pop();
        return false;
    }

    return true;
}

} // namespace ns3
