#pragma once

#include "vanetza_integration/cam/types/cam_input.h"

class CamValidator
{
public:

    static bool Validate(
        const CamInput& input);
};
