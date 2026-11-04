#include <cstdlib>
#include <iostream>

#include "vanetza_integration/vru/role/policy/vru_role_policy.h"

using vanetza_integration::vru::role::VbsState;
using vanetza_integration::vru::role::VruProfile;
using vanetza_integration::vru::role::VruRolePermissions;
using vanetza_integration::vru::role::VruRolePolicy;
using vanetza_integration::vru::role::VruRoleSnapshot;
using vanetza_integration::vru::role::VruRoleStatus;
using vanetza_integration::vru::role::VruRxDecisionReason;
using vanetza_integration::vru::role::VruTxDecisionReason;

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "ASSERTION_FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void RequirePermissions(const VruRolePermissions& actual,
                        bool transmit,
                        bool receive,
                        VruTxDecisionReason txReason,
                        VruRxDecisionReason rxReason,
                        const char* message)
{
    Require(actual.transmit == transmit, message);
    Require(actual.receive == receive, message);
    Require(actual.txReason == txReason, message);
    Require(actual.rxReason == rxReason, message);
}
}

int main()
{
    const VruRoleSnapshot off{VruRoleStatus::Off, VbsState::Idle};
    const VruRoleSnapshot inconsistentOff{
        VruRoleStatus::Off, VbsState::ActiveStandalone};
    const VruRoleSnapshot idle{VruRoleStatus::On, VbsState::Idle};
    const VruRoleSnapshot standalone{
        VruRoleStatus::On, VbsState::ActiveStandalone};
    const VruRoleSnapshot clusterLeader{
        VruRoleStatus::On, VbsState::ActiveClusterLeader};
    const VruRoleSnapshot passive{VruRoleStatus::On, VbsState::Passive};

    RequirePermissions(
        VruRolePolicy::Evaluate(off, VruProfile::Pedestrian, false),
        false, false, VruTxDecisionReason::RoleOff,
        VruRxDecisionReason::RoleOff, "ROLE_OFF must block TX and RX");

    RequirePermissions(
        VruRolePolicy::Evaluate(inconsistentOff, VruProfile::Pedestrian, false),
        false, false, VruTxDecisionReason::RoleOff,
        VruRxDecisionReason::RoleOff,
        "ROLE_OFF must dominate inconsistent VBS state");

    RequirePermissions(
        VruRolePolicy::Evaluate(idle, VruProfile::Pedestrian, false),
        false, true, VruTxDecisionReason::VbsIdle,
        VruRxDecisionReason::RoleOn, "IDLE must allow RX but block TX");

    RequirePermissions(
        VruRolePolicy::Evaluate(standalone, VruProfile::Pedestrian, false),
        true, true, VruTxDecisionReason::ProfileAllowed,
        VruRxDecisionReason::RoleOn,
        "pedestrian standalone must allow TX/RX");

    RequirePermissions(
        VruRolePolicy::Evaluate(
            standalone, VruProfile::BicyclistLightVruVehicle, false),
        true, true, VruTxDecisionReason::ProfileAllowed,
        VruRxDecisionReason::RoleOn,
        "bicyclist/light VRU vehicle standalone must allow TX/RX");

    RequirePermissions(
        VruRolePolicy::Evaluate(standalone, VruProfile::Motorcyclist, false),
        false, true, VruTxDecisionReason::UnsupportedProfile,
        VruRxDecisionReason::RoleOn,
        "motorcyclist profile must block TX");

    RequirePermissions(
        VruRolePolicy::Evaluate(standalone, VruProfile::Animal, false),
        false, true, VruTxDecisionReason::EquipmentNotConfirmed,
        VruRxDecisionReason::RoleOn,
        "animal profile without equipment confirmation must block TX");

    RequirePermissions(
        VruRolePolicy::Evaluate(standalone, VruProfile::Animal, true),
        true, true, VruTxDecisionReason::ProfileAllowed,
        VruRxDecisionReason::RoleOn,
        "animal profile with equipment confirmation may allow TX");

    RequirePermissions(
        VruRolePolicy::Evaluate(standalone, VruProfile::Unknown, false),
        false, true, VruTxDecisionReason::UnsupportedProfile,
        VruRxDecisionReason::RoleOn,
        "unknown profile must block TX");

    RequirePermissions(
        VruRolePolicy::Evaluate(clusterLeader, VruProfile::Pedestrian, false),
        false, true, VruTxDecisionReason::ClusterLeaderDeferred,
        VruRxDecisionReason::RoleOn,
        "cluster leader TX must remain deferred in this stage");

    RequirePermissions(
        VruRolePolicy::Evaluate(passive, VruProfile::Pedestrian, false),
        false, true, VruTxDecisionReason::Passive,
        VruRxDecisionReason::RoleOn,
        "PASSIVE must allow RX but block TX");

    std::cout << "VRU_ROLE_POLICY=PASS\n";

    const VruRoleStatus roles[] = {
        VruRoleStatus::Off,
        VruRoleStatus::On
    };

    const VbsState matrixVbsStates[] = {
        VbsState::Idle,
        VbsState::ActiveStandalone,
        VbsState::ActiveClusterLeader,
        VbsState::Passive
    };

    const VruProfile matrixProfiles[] = {
        VruProfile::Unknown,
        VruProfile::Pedestrian,
        VruProfile::BicyclistLightVruVehicle,
        VruProfile::Motorcyclist,
        VruProfile::Animal
    };

    const bool equipmentStates[] = {false, true};
    unsigned int matrixCases = 0;

    for (const auto role : roles)
    {
        for (const auto vbs : matrixVbsStates)
        {
            for (const auto profile : matrixProfiles)
            {
                for (const bool equipmentConfirmed : equipmentStates)
                {
                    const VruRoleSnapshot snapshot{role, vbs};

                    bool expectedTransmit = false;
                    const bool expectedReceive =
                        role == VruRoleStatus::On;

                    VruTxDecisionReason expectedTxReason =
                        VruTxDecisionReason::RoleOff;

                    const VruRxDecisionReason expectedRxReason =
                        role == VruRoleStatus::On
                            ? VruRxDecisionReason::RoleOn
                            : VruRxDecisionReason::RoleOff;

                    if (role == VruRoleStatus::On)
                    {
                        switch (vbs)
                        {
                            case VbsState::Idle:
                                expectedTxReason =
                                    VruTxDecisionReason::VbsIdle;
                                break;

                            case VbsState::ActiveClusterLeader:
                                expectedTxReason =
                                    VruTxDecisionReason::ClusterLeaderDeferred;
                                break;

                            case VbsState::Passive:
                                expectedTxReason =
                                    VruTxDecisionReason::Passive;
                                break;

                            case VbsState::ActiveStandalone:
                                switch (profile)
                                {
                                    case VruProfile::Pedestrian:
                                    case VruProfile::BicyclistLightVruVehicle:
                                        expectedTransmit = true;
                                        expectedTxReason =
                                            VruTxDecisionReason::ProfileAllowed;
                                        break;

                                    case VruProfile::Animal:
                                        expectedTransmit = equipmentConfirmed;
                                        expectedTxReason =
                                            equipmentConfirmed
                                                ? VruTxDecisionReason::ProfileAllowed
                                                : VruTxDecisionReason::EquipmentNotConfirmed;
                                        break;

                                    case VruProfile::Motorcyclist:
                                    case VruProfile::Unknown:
                                    default:
                                        expectedTxReason =
                                            VruTxDecisionReason::UnsupportedProfile;
                                        break;
                                }
                                break;
                        }
                    }

                    const auto actual = VruRolePolicy::Evaluate(
                        snapshot, profile, equipmentConfirmed);

                    RequirePermissions(
                        actual,
                        expectedTransmit,
                        expectedReceive,
                        expectedTxReason,
                        expectedRxReason,
                        "policy matrix mismatch");

                    ++matrixCases;
                }
            }
        }
    }

    Require(matrixCases == 80, "policy matrix must evaluate 80 cases");
    std::cout << "VRU_ROLE_POLICY_MATRIX_CASES=" << matrixCases << '\n';

    return EXIT_SUCCESS;
}
