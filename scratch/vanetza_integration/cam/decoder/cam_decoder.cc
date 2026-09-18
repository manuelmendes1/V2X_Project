#include "cam_decoder.h"

#include "vanetza_integration/wrappers/vanetza/cam/vanetza_cam_codec.h"
#include "vanetza_integration/core/v2x_log.h"

CamMessage CamDecoder::Decode(const ns3::PacketContext& ctx, const std::vector<uint8_t>& payload){

    ns3::PacketContext logCtx = ctx;

    logCtx.pipelineStage = ns3::PipelineStage::DECODE;

    // =========================================================================
    // Validação do payload
    // =========================================================================

    if (payload.empty()) {

        logCtx.pipelineResult = ns3::PipelineResult::FAIL;
        logCtx.reason = "EMPTY_PAYLOAD";

        // V2XWriteLog(logCtx,
        //             V2XLogLevel::WARN,
        //             "CAM_RX",
        //             "DECODE_FAIL_EMPTY",
        //             ns3::PipelineResult::FAIL,
        //             "Cannot decode an empty payload buffer");

        return {};
    }

    // =========================================================================
    // Preparação do contexto
    // =========================================================================

    logCtx.pipelineResult = ns3::PipelineResult::BEGIN;

    logCtx.payload.type = ns3::PayloadType::CAM;
    logCtx.payload.bytes = payload;
    logCtx.payload.signatureSize = 0;

    // V2XWriteLog(logCtx,
    //             V2XLogLevel::TRACE,
    //             "CAM_RX",
    //             "DECODE_BEGIN",
    //             ns3::PipelineResult::BEGIN,
    //             "Starting ASN.1 UPER decoding");

    // =========================================================================
    // Decode ASN.1 UPER
    // =========================================================================

    const auto decodeStart = V2XTimeNow();

    VanetzaCamDecodeResult result = VanetzaCamCodec::Decode(payload, ctx.timestamp);

    logCtx.durationUs = V2XElapsedMicros(decodeStart);

    if (!result.success) {

        logCtx.pipelineResult = ns3::PipelineResult::FAIL;
        logCtx.reason = result.error;

        // V2XWriteLog(logCtx,
        //             V2XLogLevel::ERROR,
        //             "CAM_RX",
        //             "DECODE_FAIL_ASN1",
        //             ns3::PipelineResult::FAIL,
        //             result.error);

        return {};
    }

    // =========================================================================
    // Resultado neutro da boundary Vanetza
    // =========================================================================

    CamMessage msg = result.message;

    // =========================================================================
    // Atualização do contexto
    // =========================================================================

    logCtx.pipelineResult = ns3::PipelineResult::OK;

    logCtx.senderStationId = msg.stationId;

    // V2XWriteLog(logCtx,
    //             V2XLogLevel::INFO,
    //             "CAM_RX",
    //             "DECODE_OK",
    //             ns3::PipelineResult::OK,
    //             "bytes=" + std::to_string(payload.size()) +
    //             " stationId=" + std::to_string(msg.stationId));

    return msg;
}
