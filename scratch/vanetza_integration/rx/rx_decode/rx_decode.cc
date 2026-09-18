#include "rx_decode.h"

#include "vanetza_integration/core/v2x_log.h"

bool RxDecode::Process(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, CamMessage& message) {
    message = CamDecoder::Decode(ctx, payload);

    if (message.stationId == 0) {
        ctx.MarkFailure();
        ctx.reason = "decode_failed";
        V2XWriteLog(ctx, V2XLogLevel::ERROR, "CAM", "DECODE_FAIL", "invalid CAM");
        return false;
    }

    return true;
}
