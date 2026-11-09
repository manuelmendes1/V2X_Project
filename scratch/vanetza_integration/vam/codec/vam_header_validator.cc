#include "vanetza_integration/vam/codec/vam_header_validator.h"

#include "MessageId.h"

namespace
{

constexpr long kVamProtocolVersion = 3;

} // namespace

VamHeaderValidationResult
VamHeaderValidator::Validate(
    const Vanetza_ITS2_ItsPduHeaderVam_t& header)
{
    // The generated ASN.1 descriptor does not enforce the VAM
    // WITH COMPONENTS restriction. Enforce it explicitly here.
    if (header.protocolVersion != kVamProtocolVersion)
    {
        return {
            false,
            "unsupported VAM protocolVersion"
        };
    }

    if (header.messageId != Vanetza_ITS2_MessageId_vam)
    {
        return {
            false,
            "invalid VAM messageID"
        };
    }

    return {
        true,
        {}
    };
}
