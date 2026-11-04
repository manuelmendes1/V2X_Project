#pragma once

namespace vanetza_integration
{
namespace vru
{
namespace role
{

enum class VruRoleStatus
{
    Off,
    On
};

enum class VbsState
{
    Idle,
    ActiveStandalone,
    ActiveClusterLeader,
    Passive
};

enum class VruProfile
{
    Unknown,
    Pedestrian,
    BicyclistLightVruVehicle,
    Motorcyclist,
    Animal
};

enum class VruTransitionResult
{
    Applied,
    NoChange,
    Rejected
};

enum class VruTxDecisionReason
{
    RoleOff,
    VbsIdle,
    ProfileAllowed,
    UnsupportedProfile,
    EquipmentNotConfirmed,
    ClusterLeaderDeferred,
    Passive
};

enum class VruRxDecisionReason
{
    RoleOff,
    RoleOn
};

struct VruRoleSnapshot
{
    VruRoleStatus role{VruRoleStatus::Off};
    VbsState vbs{VbsState::Idle};
};

struct VruRolePermissions
{
    bool transmit{false};
    bool receive{false};
    VruTxDecisionReason txReason{VruTxDecisionReason::RoleOff};
    VruRxDecisionReason rxReason{VruRxDecisionReason::RoleOff};
};

} // namespace role
} // namespace vru
} // namespace vanetza_integration
