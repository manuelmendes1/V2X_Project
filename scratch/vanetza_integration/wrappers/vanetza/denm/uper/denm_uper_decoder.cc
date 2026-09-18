#include "denm_uper_decoder.h"

extern "C"
{
#include "asn_application.h"
#include "asn_internal.h"
}

DenmUperDecodeResult
DenmUperDecoder::Decode(
    const std::vector<uint8_t>& payload)
{
    DenmUperDecodeResult result;

    if (payload.empty())
    {
        result.error = "empty payload";
        return result;
    }

    DENM_t* rawDenm = nullptr;

    const asn_dec_rval_t rval =
        uper_decode(
            nullptr,
            &asn_DEF_DENM,
            reinterpret_cast<void**>(&rawDenm),
            payload.data(),
            payload.size(),
            0,
            0);

    if (rval.code != RC_OK || rawDenm == nullptr)
    {
        result.error = "ASN.1 decode failed";

        if (rawDenm)
        {
            ASN_STRUCT_FREE(asn_DEF_DENM, rawDenm);
        }

        return result;
    }

    result.success = true;
    result.consumedBits =
        static_cast<std::size_t>(rval.consumed);

    result.denm.reset(rawDenm);

    return result;
}
