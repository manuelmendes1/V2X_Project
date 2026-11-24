#include "rx_decode.h"

#include "vanetza_integration/core/v2x_log.h"

#include <string>

bool RxDecode::Process(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, CamMessage& message) {
    message = CamDecoder::Decode(ctx, payload);

    if (message.stationId == 0) {
        ctx.MarkFailure();
        ctx.reason = "decode_failed";
        V2XWriteLog(ctx, V2XLogLevel::ERROR, "CAM", "DECODE_FAIL", "invalid CAM");
        return false;
    }

    if (ctx.senderStationId == 0) {
        ctx.MarkFailure();
        ctx.reason = "cam_sender_identity_missing";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "DECODE_FAIL",
            "resolved sender station identity is missing");

        return false;
    }

    if (message.stationId != ctx.senderStationId) {
        ctx.MarkDrop();
        ctx.reason = "cam_sender_identity_mismatch";

        V2XWriteLog(
            ctx,
            V2XLogLevel::WARN,
            "CAM",
            "DECODE_DROP",
            "sender_station=" +
                std::to_string(ctx.senderStationId) +
                " cam_station=" +
                std::to_string(message.stationId));

        return false;
    }

    return true;
}
