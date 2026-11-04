#include "vanetza_integration/vru/role/policy/vru_role_policy.h"

namespace vanetza_integration
{
namespace vru
{
namespace role
{

VruRolePermissions VruRolePolicy::Evaluate(
    const VruRoleSnapshot& snapshot,
    VruProfile profile,
    bool animalEquipmentConfirmed) noexcept
{
    if (snapshot.role == VruRoleStatus::Off)
    {
        return {
            false,
            false,
            VruTxDecisionReason::RoleOff,
            VruRxDecisionReason::RoleOff
        };
    }

    if (snapshot.vbs == VbsState::Idle)
    {
        return {
            false,
            true,
            VruTxDecisionReason::VbsIdle,
            VruRxDecisionReason::RoleOn
        };
    }

    if (snapshot.vbs == VbsState::ActiveClusterLeader)
    {
        return {
            false,
            true,
            VruTxDecisionReason::ClusterLeaderDeferred,
            VruRxDecisionReason::RoleOn
        };
    }

    if (snapshot.vbs == VbsState::Passive)
    {
        return {
            false,
            true,
            VruTxDecisionReason::Passive,
            VruRxDecisionReason::RoleOn
        };
    }

    switch (profile)
    {
        case VruProfile::Pedestrian:
        case VruProfile::BicyclistLightVruVehicle:
            return {
                true,
                true,
                VruTxDecisionReason::ProfileAllowed,
                VruRxDecisionReason::RoleOn
            };

        case VruProfile::Animal:
            if (animalEquipmentConfirmed)
            {
                return {
                    true,
                    true,
                    VruTxDecisionReason::ProfileAllowed,
                    VruRxDecisionReason::RoleOn
                };
            }

            return {
                false,
                true,
                VruTxDecisionReason::EquipmentNotConfirmed,
                VruRxDecisionReason::RoleOn
            };

        case VruProfile::Motorcyclist:
        case VruProfile::Unknown:
        default:
            return {
                false,
                true,
                VruTxDecisionReason::UnsupportedProfile,
                VruRxDecisionReason::RoleOn
            };
    }
}

} // namespace role
} // namespace vru
} // namespace vanetza_integration
