#include "cam_uper_encoder.h"

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

CamUperResult
CamUperEncoder::Encode(
    const CAM_t& cam)
{
    CamUperResult result;

    std::array<uint8_t, kMaxBufferSize> buffer;

    const asn_enc_rval_t rval =
        uper_encode_to_buffer(
            &asn_DEF_CAM,
            nullptr,
            &cam,
            buffer.data(),
            buffer.size());

    if (rval.encoded <= 0)
    {
        result.success = false;

        if (rval.failed_type && rval.failed_type->name)
        {
            result.failedType = rval.failed_type->name;
        }
        else
        {
            result.failedType = "unknown";
        }

        return result;
    }

    result.success = true;
    result.encodedBits = rval.encoded;

    const std::size_t encodedSize =
        (rval.encoded + 7) / 8;

    result.bytes.assign(
        buffer.begin(),
        buffer.begin() + encodedSize);

    return result;
}
