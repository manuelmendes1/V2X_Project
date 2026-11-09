#pragma once

#include <optional>
#include <string>

#include "vanetza_integration/vru/role/types/vru_role_types.h"

namespace vanetza_integration
{
namespace vru
{

// Maps source-level SUMO metadata to an approved profile. This operation
// does not activate a VRU role or assign an ITS identity.
class SumoVruProfileMapper
{
public:
    static role::VruProfile Map(
        const std::optional<std::string>& sumoVClass) noexcept;
};

} // namespace vru
} // namespace vanetza_integration
