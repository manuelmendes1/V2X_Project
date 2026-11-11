#pragma once

#include "vanetza_integration/cam/types/cam_input.h"

class CamValidator
{
public:
    static bool ValidateWgs84(
        double latitudeDeg,
        double longitudeDeg);

    static bool Validate(
        const CamInput& input);
};
