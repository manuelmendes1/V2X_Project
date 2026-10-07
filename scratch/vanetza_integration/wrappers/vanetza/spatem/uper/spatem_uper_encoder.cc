#include "spatem_uper_encoder.h"

#include <array>

extern "C"
{
#include "asn_application.h"
#include "asn_internal.h"
#include "constr_TYPE.h"
}

namespace
{
constexpr std::size_t kMaxBufferSize = 4096;
}

SpatemUperEncodeResult
SpatemUperEncoder::Encode(
    const SPATEM_t& spatem)
{
    SpatemUperEncodeResult result;

    std::array<uint8_t, kMaxBufferSize> buffer{};

    char errorBuffer[256] = {};
    size_t errorLength = sizeof(errorBuffer);

    if (asn_check_constraints(
            &asn_DEF_SPATEM,
            &spatem,
            errorBuffer,
            &errorLength) != 0)
    {
        result.failedType =
            errorBuffer[0] != '\0'
                ? errorBuffer
                : "asn1_constraints_failed";
        return result;
    }

    const asn_enc_rval_t rval =
        uper_encode_to_buffer(
            &asn_DEF_SPATEM,
            nullptr,
            &spatem,
            buffer.data(),
            buffer.size());

    if (rval.encoded <= 0)
    {
        result.failedType =
            rval.failed_type &&
            rval.failed_type->name
                ? rval.failed_type->name
                : "unknown";
        return result;
    }

    result.success = true;
    result.encodedBits =
        static_cast<std::size_t>(rval.encoded);

    const std::size_t encodedSize =
        (result.encodedBits + 7) / 8;

    result.bytes.assign(
        buffer.begin(),
        buffer.begin() + encodedSize);

    return result;
}
