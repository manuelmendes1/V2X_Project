#include "vanetza_cam_codec.h"

#include "cam_asn1_builder.h"
#include "cam_converter.h"
#include "uper/cam_uper_decoder.h"
#include "uper/cam_uper_encoder.h"

VanetzaCamEncodeResult
VanetzaCamCodec::Encode(const CamInput& input)
{
    VanetzaCamEncodeResult result;

    auto cam = CamAsn1Builder::Build(input);

    if (!cam)
    {
        result.failedType = "asn1_alloc_failed";
        return result;
    }

    const auto encoded = CamUperEncoder::Encode(*cam);

    if (!encoded.success)
    {
        result.failedType = encoded.failedType;
        return result;
    }

    result.success = true;
    result.bytes = encoded.bytes;

    return result;
}

VanetzaCamDecodeResult
VanetzaCamCodec::Decode(
    const std::vector<uint8_t>& payload,
    uint64_t timestamp)
{
    VanetzaCamDecodeResult result;

    const auto decoded = CamUperDecoder::Decode(payload);

    if (!decoded.success || !decoded.cam)
    {
        result.error = decoded.error.empty()
            ? "ASN.1 decode failed"
            : decoded.error;

        return result;
    }

    result.message =
        CamConverter::ToMessage(*decoded.cam, timestamp);

    result.success = true;

    return result;
}
