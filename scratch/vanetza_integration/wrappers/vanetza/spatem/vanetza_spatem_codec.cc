#include "vanetza_spatem_codec.h"

#include "spatem_asn1_builder.h"
#include "spatem_converter.h"
#include "uper/spatem_uper_decoder.h"
#include "uper/spatem_uper_encoder.h"

VanetzaSpatemEncodeResult
VanetzaSpatemCodec::Encode(
    const SpatemInput& input)
{
    VanetzaSpatemEncodeResult result;

    auto spatem =
        SpatemAsn1Builder::Build(input);

    if (!spatem)
    {
        result.error =
            "SPATEM ASN.1 build failed";
        return result;
    }

    const auto encoded =
        SpatemUperEncoder::Encode(*spatem);

    if (!encoded.success)
    {
        result.error =
            encoded.failedType;
        return result;
    }

    result.success = true;
    result.bytes = encoded.bytes;
    result.encodedBits =
        encoded.encodedBits;

    return result;
}

VanetzaSpatemDecodeResult
VanetzaSpatemCodec::Decode(
    const std::vector<uint8_t>& payload)
{
    VanetzaSpatemDecodeResult result;

    const auto decoded =
        SpatemUperDecoder::Decode(payload);

    if (!decoded.success ||
        !decoded.spatem)
    {
        result.error =
            decoded.error.empty()
                ? "SPATEM ASN.1 decode failed"
                : decoded.error;
        return result;
    }

    if (decoded.spatem->header.protocolVersion != 2)
    {
        result.error =
            "invalid SPATEM protocolVersion";
        return result;
    }

    if (decoded.spatem->header.messageID !=
        ItsPduHeader__messageID_spatem)
    {
        result.error =
            "invalid SPATEM messageID";
        return result;
    }

    result.message =
        SpatemConverter::ToMessage(
            *decoded.spatem);

    result.consumedBits =
        decoded.consumedBits;
    result.success = true;

    return result;
}
