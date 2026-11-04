#pragma once

#include "vanetza_integration/vru/role/types/vru_role_types.h"

namespace vanetza_integration
{
namespace vru
{
namespace role
{

class VruRolePolicy
{
public:
    static VruRolePermissions Evaluate(
        const VruRoleSnapshot& snapshot,
        VruProfile profile,
        bool animalEquipmentConfirmed) noexcept;
};

} // namespace role
} // namespace vru
} // namespace vanetza_integration
