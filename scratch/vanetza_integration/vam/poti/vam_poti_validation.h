#pragma once

#include <string>

#include "vanetza_integration/vam/poti/vam_poti_sample.h"

class VamPotiValidation
{
public:
    static bool Validate(
        const VamPotiSample& sample,
        std::string& error);
};
