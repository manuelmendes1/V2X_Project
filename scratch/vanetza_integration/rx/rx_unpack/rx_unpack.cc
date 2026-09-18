#include "rx_unpack.h"

#include "vanetza_integration/wrappers/vanetza/cam/uper/cam_uper_decoder.h"
#include "vanetza_integration/core/v2x_log.h"

bool
RxUnpack::ExtractRaw(
    ns3::PacketContext& ctx,
    const std::vector<uint8_t>& buffer,
    CamPayloadParts& frame)
{
    if (buffer.empty())
    {
        ctx.MarkFailure();
        ctx.reason = "empty_buffer";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "EXTRACT_FAIL",
            "empty buffer");

        return false;
    }

    //----------------------------------------------------------------------
    // Decode only the CAM prefix.
    //
    // uper_decode() returns consumed bits even when additional bytes follow
    // the CAM. This gives us the protocol-native CAM boundary without any
    // proprietary length/header field.
    //----------------------------------------------------------------------
    CamUperDecodeResult decodeResult =
        CamUperDecoder::Decode(buffer);

    if (!decodeResult.success || !decodeResult.cam)
    {
        ctx.MarkFailure();
        ctx.reason = "cam_decode_failed";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "EXTRACT_FAIL",
            decodeResult.error);

        return false;
    }

    if (decodeResult.consumedBits == 0)
    {
        ctx.MarkFailure();
        ctx.reason = "invalid_consumed_bits";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "EXTRACT_FAIL",
            "decoder consumed zero bits");

        return false;
    }

    const std::size_t camSize =
        (decodeResult.consumedBits + 7) / 8;

    if (camSize > buffer.size())
    {
        ctx.MarkFailure();
        ctx.reason = "cam_size_exceeds_buffer";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "EXTRACT_FAIL",
            "decoded CAM size exceeds buffer");

        return false;
    }

    //----------------------------------------------------------------------
    // Application wire format:
    //
    //   [CAM ASN.1 bytes][signature bytes]
    //
    // No proprietary header is present.
    //----------------------------------------------------------------------
    frame.payload.assign(
        buffer.begin(),
        buffer.begin() + camSize);

    frame.signature.assign(
        buffer.begin() + camSize,
        buffer.end());

    if (frame.payload.empty())
    {
        ctx.MarkFailure();
        ctx.reason = "empty_cam_payload";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "EXTRACT_FAIL",
            "CAM payload is empty");

        return false;
    }

    if (frame.signature.empty())
    {
        ctx.MarkFailure();
        ctx.reason = "empty_signature";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "CAM",
            "EXTRACT_FAIL",
            "signature is empty");

        return false;
    }

    ctx.payload.type = ns3::PayloadType::CAM;
    ctx.payload.bytes = buffer;
    ctx.payload.signatureSize =
        static_cast<uint32_t>(frame.signature.size());

    return true;
}
