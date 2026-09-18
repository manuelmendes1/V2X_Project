#include "denm_uper_encoder.h"

#include <array>

extern "C"
{
#include "asn_application.h"
#include "asn_internal.h"
}

namespace
{
    constexpr std::size_t kMaxBufferSize = 2048;
}

DenmUperEncodeResult
DenmUperEncoder::Encode(const DENM_t& denm)
{
    DenmUperEncodeResult result;

    std::array<uint8_t, kMaxBufferSize> buffer{};

    const asn_enc_rval_t rval =
        uper_encode_to_buffer(
            &asn_DEF_DENM,
            nullptr,
            &denm,
            buffer.data(),
            buffer.size());

    if (rval.encoded <= 0)
    {
        result.failedType =
            rval.failed_type && rval.failed_type->name
                ? rval.failed_type->name
                : "unknown";

        return result;
    }

    result.success = true;
    result.encodedBits =
        static_cast<std::size_t>(rval.encoded);

    const std::size_t encodedSize =
        (rval.encoded + 7) / 8;

    result.bytes.assign(
        buffer.begin(),
        buffer.begin() + encodedSize);

    return result;
}
