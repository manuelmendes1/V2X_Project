#include "cam_validator.h"

#include <cmath>

bool
CamValidator::ValidateWgs84(
    double latitudeDeg,
    double longitudeDeg)
{
    return std::isfinite(latitudeDeg) &&
           std::isfinite(longitudeDeg) &&
           latitudeDeg >= -90.0 &&
           latitudeDeg <= 90.0 &&
           longitudeDeg >= -180.0 &&
           longitudeDeg <= 180.0;
}

bool
CamValidator::Validate(
    const CamInput& input)
{
    return ValidateWgs84(
        input.latitude,
        input.longitude);
}
