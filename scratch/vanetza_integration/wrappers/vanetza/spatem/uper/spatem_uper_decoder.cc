#include "spatem_uper_decoder.h"

extern "C"
{
#include "asn_application.h"
#include "asn_internal.h"
}

SpatemUperDecodeResult
SpatemUperDecoder::Decode(
    const std::vector<uint8_t>& payload)
{
    SpatemUperDecodeResult result;

    if (payload.empty())
    {
        result.error = "empty payload";
        return result;
    }

    SPATEM_t* rawSpatem = nullptr;

    const asn_dec_rval_t rval =
        uper_decode(
            nullptr,
            &asn_DEF_SPATEM,
            reinterpret_cast<void**>(&rawSpatem),
            payload.data(),
            payload.size(),
            0,
            0);

    if (rval.code != RC_OK ||
        rawSpatem == nullptr)
    {
        result.error =
            "ASN.1 decode failed";

        if (rawSpatem)
        {
            ASN_STRUCT_FREE(
                asn_DEF_SPATEM,
                rawSpatem);
        }

        return result;
    }

    result.success = true;
    result.consumedBits =
        static_cast<std::size_t>(rval.consumed);
    result.spatem.reset(rawSpatem);

    return result;
}
