#include "denm_service.h"

#include "vanetza_integration/context/packet/packet_id_manager.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/denm/action/denm_action_id_manager.h"
#include "vanetza_integration/wrappers/vanetza/denm/vanetza_denm_codec.h"

namespace ns3
{

std::vector<uint8_t>
DenmService::GenerateRawPayload(
    DenmGenerationContext& context,
    const std::string& reason)
{
    if (!context.packetContext)
    {
        return {};
    }

    PacketContext& ctx =
        *context.packetContext;

    if (context.denmInput.actionId.sequenceNumber == 0)
    {
        ctx.MarkFailure();
        ctx.reason = "action_id_unavailable";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "GENERATE_FAIL",
            "DENM ActionID unavailable");

        return {};
    }

    auto& packetIds =
        PacketIdManager::Instance();

    ctx.packetId =
        packetIds.NextPacketId();

    ctx.vehiclePacketId =
        packetIds.NextVehiclePacketId(
            ctx.senderVehId);

    //----------------------------------------------------------------------
    // Deliberately independent DENM packet sequence.
    //
    // This is PacketContext observability metadata and is NOT the
    // DENM ActionID sequenceNumber.
    //----------------------------------------------------------------------

    ctx.sequenceId =
        packetIds.NextSequenceId(
            ctx.senderVehId,
            PayloadType::DENM);

    //----------------------------------------------------------------------
    // Step 1/8: GENERATE
    //----------------------------------------------------------------------

    ctx.AdvancePipeline(
        PipelineStage::GENERATE);

    ctx.reason =
        reason;

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "DENM",
        "GENERATE_OK",
        "reason=" +
        reason +
        " action_seq=" +
        std::to_string(
            context.denmInput.actionId.sequenceNumber) +
        " packet_seq=" +
        std::to_string(
            ctx.sequenceId));

    //----------------------------------------------------------------------
    // Step 2/8: ENCODE
    //----------------------------------------------------------------------

    ctx.AdvancePipeline(
        PipelineStage::ENCODE);

    const auto encoded =
        VanetzaDenmCodec::Encode(
            context.denmInput);

    if (!encoded.success ||
        encoded.bytes.empty())
    {
        ctx.MarkFailure();
        ctx.reason =
            encoded.failedType.empty()
                ? "denm_encode_failed"
                : encoded.failedType;

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "ENCODE_FAIL",
            ctx.reason);

        return {};
    }

    ctx.payload.type =
        PayloadType::DENM;

    ctx.payload.bytes =
        encoded.bytes;

    ctx.payload.signatureSize =
        0;

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "DENM",
        "ENCODE_OK",
        "bytes=" +
        std::to_string(
            encoded.bytes.size()) +
        " action_seq=" +
        std::to_string(
            context.denmInput.actionId.sequenceNumber));

    //----------------------------------------------------------------------
    // Security/signature is handled later in the DENM security layer.
    // Keep ENCODE successful here; the raw DENM payload is handed to the
    // security path where the V3 security envelope is attached.
    //----------------------------------------------------------------------

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "DENM",
        "SIGN_DEFERRED",
        "DENM security envelope handled by DENM security layer");

    return encoded.bytes;
}

} // namespace ns3
