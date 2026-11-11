#include "cam_encoder.h"

#include "vanetza_integration/wrappers/vanetza/cam/vanetza_cam_codec.h"
#include "vanetza_integration/cam/validation/cam_validator.h"

#include "vanetza_integration/core/v2x_log.h"

#include <utility>

// ============================================================================
// Encode CAM
// ============================================================================

std::vector<uint8_t> CamEncoder::Encode(const ns3::PacketContext& ctx, const CamInput& input) {

    ns3::PacketContext logCtx = ctx;

    //=========================================================================
    // Context enrichment
    //=========================================================================
    //
    // O encoder confirma o Station ID do emissor local. A identidade remota
    // não existe durante a transmissão.
    //
    //=========================================================================

    logCtx.senderStationId = input.stationId;

    //=========================================================================
    // Validation
    //=========================================================================

    if (!CamValidator::Validate(input)) {

        logCtx.MarkFailure();
        logCtx.reason = "invalid_position";

        V2XWriteLog(logCtx, V2XLogLevel::DEBUG, "CAM", "ENCODE_FAIL", "Invalid WGS84 position: latitude must be within [-90, 90] and longitude within [-180, 180] degrees; both must be finite");

        return {};
    }

    //=========================================================================
    // Encode through Vanetza CAM wrapper
    //=========================================================================

    V2XWriteLog(logCtx, V2XLogLevel::DEBUG, "CAM", "ENCODE_BEGIN");

    const auto start = V2XTimeNow();

    auto result = VanetzaCamCodec::Encode(input);

    logCtx.durationUs = V2XElapsedMicros(start);

    if (!result.success) {

        logCtx.MarkFailure();
        logCtx.reason = result.failedType;

        V2XWriteLog(logCtx, V2XLogLevel::DEBUG, "CAM", "ENCODE_FAIL", "failed_type=" + result.failedType);

        return {};
    }

    //=========================================================================
    // Update context
    //=========================================================================

    logCtx.payload.type = ns3::PayloadType::CAM;
    logCtx.payload.bytes = result.bytes;
    logCtx.payload.signatureSize = 0;

    logCtx.MarkSuccess();

    V2XWriteLog(logCtx, V2XLogLevel::DEBUG, "CAM", "ENCODE_OK", "bytes=" + std::to_string(result.bytes.size()));

    return std::move(result.bytes);
}
