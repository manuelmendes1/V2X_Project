#include <iomanip>
#include <cstdint>
#include "vanetza_integration/cpm/service/cpm_service.h"

#include <sstream>
#include <utility>

#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"

namespace ns3
{
namespace
{

template <typename Objects>
std::string ObjectIds(const Objects& objects)
{
    std::ostringstream out;
    bool first = true;

    for (const auto& object : objects)
    {
        if (!first)
        {
            out << ',';
        }

        out << object.objectId;
        first = false;
    }

    return first ? "-" : out.str();
}

std::string PayloadFingerprint(
    const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t hash = 14695981039346656037ULL;

    for (const std::uint8_t byte : bytes)
    {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string InputSummary(const CpmInput& input)
{
    return "station=" + std::to_string(input.station.stationId) +
        " reference_time_its_ms=" +
            std::to_string(input.referenceTimeItsMs) +
        " known_objects=" + std::to_string(input.knownObjectCount) +
        " included_objects=" +
            std::to_string(input.perceivedObjects.size()) +
        " object_ids=" + ObjectIds(input.perceivedObjects);
}

} // namespace

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

    // Build the request once. Its metadata must be available to generation
    // and encoding logs and the same request must reach the transport gateway.
    const BtpTxRequest txRequest =
        MakeCpmBtpRequest(trafficClass);
    SetPacketContextTxBtpMetadata(ctx, txRequest);

    ctx.payload.Clear();
    ctx.AdvancePipeline(PipelineStage::GENERATE);

    if (input.station.stationId == 0 ||
        ctx.senderStationId == 0 ||
        input.station.stationId != ctx.senderStationId)
    {
        ctx.MarkFailure();
        ctx.reason = "cpm_sender_station_id_mismatch";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CPM",
            "CPM_GENERATE_FAIL",
            ctx.reason + " " + InputSummary(input));
        return false;
    }

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_GENERATE_OK",
        InputSummary(input));

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

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CPM",
            "CPM_ENCODE_FAIL",
            ctx.reason + " " + InputSummary(input) +
                " encoded_bits=" + std::to_string(encoded.encodedBits) +
                " encoded_bytes=" + std::to_string(encoded.bytes.size()));
        return false;
    }

    ctx.payload.bytes = encoded.bytes;
    ctx.payload.type = PayloadType::CPM;
    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_ENCODE_OK",
        InputSummary(input) +
            " encoded_bits=" + std::to_string(encoded.encodedBits) +
            " encoded_bytes=" + std::to_string(encoded.bytes.size()) +
            " payload_fnv1a64=" + PayloadFingerprint(encoded.bytes));

    // The access layer owns the successful asynchronous TX lifecycle.
    // On failure, this service removes the context it pushed.
    TxBridgeContext::Push(context);
    ctx.AdvancePipeline(PipelineStage::BTP_TX);

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_BTP_TX_BEGIN",
        InputSummary(input) +
            " encoded_bytes=" + std::to_string(encoded.bytes.size()));

    const bool sent =
        gateway.Send(ctx, txRequest, encoded.bytes);

    if (!sent)
    {
        ctx.MarkFailure();

        if (ctx.reason.empty())
        {
            ctx.reason = "cpm_transport_send_failed";
        }

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CPM",
            "CPM_BTP_TX_FAIL",
            ctx.reason + " " + InputSummary(input));

        TxBridgeContext::Pop();
        return false;
    }

    return true;
}

} // namespace ns3
