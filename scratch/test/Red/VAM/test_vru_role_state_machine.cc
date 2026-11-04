#include <array>
#include <cstdlib>
#include <iostream>

#include "vanetza_integration/vru/role/state_machine/vru_role_state_machine.h"

using vanetza_integration::vru::role::VbsState;
using vanetza_integration::vru::role::VruRoleStateMachine;
using vanetza_integration::vru::role::VruRoleStatus;
using vanetza_integration::vru::role::VruTransitionResult;

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

constexpr std::array<VbsState, 4> VbsStates{
    VbsState::Idle,
    VbsState::ActiveStandalone,
    VbsState::ActiveClusterLeader,
    VbsState::Passive
};
}

int main()
{
    VruRoleStateMachine machine;

    Require(machine.state().role == VruRoleStatus::Off,
            "initial role must be OFF");
    Require(machine.state().vbs == VbsState::Idle,
            "initial VBS state must be IDLE");

    Require(machine.activate() == VruTransitionResult::Applied,
            "OFF -> ON must be applied");
    Require(machine.state().role == VruRoleStatus::On,
            "activation must set role ON");
    Require(machine.state().vbs == VbsState::Idle,
            "activation must not implicitly activate VBS");
    Require(machine.activate() == VruTransitionResult::NoChange,
            "repeated activation must be idempotent");

    for (const auto vbsState : VbsStates)
    {
        const auto expectedResult =
            machine.state().vbs == vbsState
                ? VruTransitionResult::NoChange
                : VruTransitionResult::Applied;

        const auto actualResult = machine.setVbsState(vbsState);

        Require(actualResult == expectedResult,
                "ON role must apply VBS changes and preserve idempotence");

        Require(machine.state().role == VruRoleStatus::On,
                "VBS transition must not change role");
        Require(machine.state().vbs == vbsState,
                "VBS transition must retain requested state");

        Require(machine.setVbsState(vbsState) ==
                    VruTransitionResult::NoChange,
                "repeated VBS transition must be idempotent");
    }

    for (const auto vbsState : VbsStates)
    {
        VruRoleStateMachine candidate;

        Require(candidate.activate() == VruTransitionResult::Applied,
                "candidate activation must succeed");

        Require(candidate.setVbsState(vbsState) ==
                    (vbsState == VbsState::Idle
                         ? VruTransitionResult::NoChange
                         : VruTransitionResult::Applied),
                "candidate must enter each valid VBS state");

        Require(candidate.deactivate() == VruTransitionResult::Applied,
                "deactivation from every VBS state must be applied");
        Require(candidate.state().role == VruRoleStatus::Off,
                "deactivation must set role OFF");
        Require(candidate.state().vbs == VbsState::Idle,
                "deactivation must reset VBS state to IDLE");
        Require(candidate.deactivate() == VruTransitionResult::NoChange,
                "repeated deactivation must be idempotent");

        for (const auto rejectedState : VbsStates)
        {
            if (rejectedState == VbsState::Idle)
            {
                continue;
            }

            Require(candidate.setVbsState(rejectedState) ==
                        VruTransitionResult::Rejected,
                    "OFF role must reject every non-IDLE VBS state");
            Require(candidate.state().role == VruRoleStatus::Off &&
                        candidate.state().vbs == VbsState::Idle,
                    "rejected transition must preserve OFF/IDLE state");
        }
    }

    std::cout << "VRU_ROLE_STATE_MACHINE=PASS\n";
    return EXIT_SUCCESS;
}
