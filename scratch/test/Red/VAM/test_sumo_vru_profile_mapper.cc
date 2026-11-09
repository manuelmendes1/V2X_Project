#include "vanetza_integration/vru/profile/sumo_vru_profile_mapper.h"

#include <array>
#include <iostream>
#include <optional>
#include <string>

using vanetza_integration::vru::SumoVruProfileMapper;
using vanetza_integration::vru::role::VruProfile;

namespace
{

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    return condition;
}

} // namespace

int main()
{
    bool passed = true;

    passed = Check(
        SumoVruProfileMapper::Map(
            std::optional<std::string>{"pedestrian"}) == VruProfile::Pedestrian,
        "SUMO_PEDESTRIAN_MAPS_TO_PROFILE_1") && passed;

    passed = Check(
        SumoVruProfileMapper::Map(std::nullopt) == VruProfile::Unknown,
        "MISSING_SUMO_VCLASS_MAPS_TO_UNKNOWN") && passed;

    passed = Check(
        SumoVruProfileMapper::Map(std::string{}) == VruProfile::Unknown,
        "EMPTY_SUMO_VCLASS_MAPS_TO_UNKNOWN") && passed;

    const std::array<std::string, 8> unapprovedClasses{
        "bicycle", "moped", "motorcycle", "scooter", "passenger",
        "animal", "Pedestrian", "unrecognized"};
    for (const auto& sumoVClass : unapprovedClasses)
    {
        passed = Check(
            SumoVruProfileMapper::Map(
                std::optional<std::string>{sumoVClass}) == VruProfile::Unknown,
            "UNAPPROVED_SUMO_VCLASS_REMAINS_UNKNOWN") && passed;
    }

    std::cout << "SUMO_VRU_PROFILE_MAPPER="
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}
