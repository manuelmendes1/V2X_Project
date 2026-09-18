#include "cam_service.h"

#include "vanetza_integration/cam/encoder/cam_encoder.h"
#include "vanetza_integration/context/packet/packet_id_manager.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/security/cam_security.h"

#include <ns3/simulator.h>

std::vector<uint8_t> CamService::GenerateRawPayload(ns3::CamGenerationContext& context, const std::string& reason) {

    ns3::PacketContext& ctx = *context.packetContext;

    auto& packetIds = ns3::PacketIdManager::Instance();
    ctx.packetId        = packetIds.NextPacketId();
    ctx.vehiclePacketId = packetIds.NextVehiclePacketId(ctx.senderVehId);
    ctx.sequenceId      = packetIds.NextSequenceId(ctx.senderVehId, ns3::PayloadType::CAM);

    //=========================================================================
    // Step 1/8: GENERATE
    //=========================================================================
    ctx.AdvancePipeline(ns3::PipelineStage::GENERATE);
    ctx.reason = reason;
    ctx.MarkSuccess();
    V2XWriteLog(ctx, V2XLogLevel::DEBUG, "CAM", "GENERATE_OK", "reason=" + reason);

    //=========================================================================
    // Step 2/8: ENCODE
    //=========================================================================
    ctx.AdvancePipeline(ns3::PipelineStage::ENCODE);
    std::vector<uint8_t> payload = CamEncoder::Encode(ctx, context.camInput);
    if (payload.empty()) {
        ctx.MarkFailure();
        ctx.reason = "encode_failed";
        V2XWriteLog(ctx, V2XLogLevel::ERROR, "CAM", "ENCODE_FAIL", "ASN.1 encoding failed");
        return {};
    }
    ctx.payload.type = ns3::PayloadType::CAM;
    ctx.payload.bytes = payload;
    ctx.payload.signatureSize = 0;
    ctx.MarkSuccess();

    //=========================================================================
    // Step 3/8: SIGN
    //=========================================================================
    ctx.AdvancePipeline(ns3::PipelineStage::SIGN);
    std::vector<uint8_t> signature = CamSecurity::Sign(ctx, payload, context.security.privateKey);
    if (signature.empty()) {
        ctx.MarkFailure();
        ctx.reason = "sign_failed";
        V2XWriteLog(ctx, V2XLogLevel::ERROR, "CAM", "SIGN_FAIL", "Signature generation failed");
        return {};
    }
    ctx.payload.signatureSize = static_cast<uint32_t>(signature.size());
    ctx.MarkSuccess();

    //=========================================================================
    // Application payload
    //
    // Fase 9: no proprietary framing.
    //
    // Wire format at BTP Application:
    //   [CAM ASN.1 bytes][signature bytes]
    //
    // packetId / vehiclePacketId / sequenceId remain internal PacketContext
    // metadata and are not serialized onto the wire.
    //=========================================================================
    std::vector<uint8_t> rawPayload;
    rawPayload.reserve(payload.size() + signature.size());

    rawPayload.insert(
        rawPayload.end(),
        payload.begin(),
        payload.end());

    rawPayload.insert(
        rawPayload.end(),
        signature.begin(),
        signature.end());

    if (rawPayload.empty()) {
        ctx.MarkFailure();
        ctx.reason = "raw_payload_empty";
        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "RAW_PAYLOAD_FAIL",
            "CAM payload plus signature is empty");
        return {};
    }

    ctx.payload.type = ns3::PayloadType::CAM;
    ctx.payload.bytes = rawPayload;
    ctx.payload.signatureSize = static_cast<uint32_t>(signature.size());

    // O pipeline permanece no step 3/8 (SIGN). Os steps 4-8
    // (BTP_TX -> GEONET_TX -> ACCESS_TX -> SEND -> TX_COMPLETE)
    // são executados pelo transporte (CamSender::SendRaw).
    return rawPayload;
}
