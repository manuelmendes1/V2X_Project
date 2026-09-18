#include "cam_uper_decoder.h"

extern "C"
{
#include "asn_application.h"
#include "asn_internal.h"
}

CamUperDecodeResult
CamUperDecoder::Decode(
    const std::vector<uint8_t>& payload)
{
    CamUperDecodeResult result;

    if (payload.empty())
    {
        result.error = "empty payload";
        return result;
    }

    CAM_t* rawCam = nullptr;

    const asn_dec_rval_t rval =
        uper_decode(
            nullptr,
            &asn_DEF_CAM,
            reinterpret_cast<void**>(&rawCam),
            payload.data(),
            payload.size(),
            0,
            0);

    if (rval.code != RC_OK || rawCam == nullptr)
    {
        result.error = "ASN.1 decode failed";

        if (rawCam)
        {
            ASN_STRUCT_FREE(asn_DEF_CAM, rawCam);
        }

        return result;
    }

    result.success = true;
    result.consumedBits = rval.consumed;
    result.cam.reset(rawCam);

    return result;
}
