#include "cam_validator.h"

#include <cmath>

namespace
{
    constexpr double kPositionThreshold = 0.0001;
}

bool CamValidator::Validate(
    const CamInput& input)
{
    return !(
        std::abs(input.latitude) < kPositionThreshold &&
        std::abs(input.longitude) < kPositionThreshold);
}
