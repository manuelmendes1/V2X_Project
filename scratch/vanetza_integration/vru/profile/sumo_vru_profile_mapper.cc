#include "vanetza_integration/vru/profile/sumo_vru_profile_mapper.h"

namespace vanetza_integration
{
namespace vru
{

role::VruProfile SumoVruProfileMapper::Map(
    const std::optional<std::string>& sumoVClass) noexcept
{
    if (sumoVClass.has_value() && *sumoVClass == "pedestrian")
    {
        return role::VruProfile::Pedestrian;
    }

    // Do not infer bicyclist, motorcyclist, animal, or equipment profiles
    // from SUMO vClass. Those mappings need explicit domain approval.
    return role::VruProfile::Unknown;
}

} // namespace vru
} // namespace vanetza_integration
