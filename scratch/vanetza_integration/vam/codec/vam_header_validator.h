#ifndef VANETZA_INTEGRATION_VAM_CODEC_VAM_HEADER_VALIDATOR_H
#define VANETZA_INTEGRATION_VAM_CODEC_VAM_HEADER_VALIDATOR_H

#include "ItsPduHeaderVam.h"

#include <string>

struct VamHeaderValidationResult
{
    bool success = false;
    std::string error;
};

class VamHeaderValidator
{
public:
    static VamHeaderValidationResult Validate(
        const Vanetza_ITS2_ItsPduHeaderVam_t& header);
};

#endif
