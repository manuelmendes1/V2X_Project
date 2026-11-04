#include "vanetza_integration/vru/role/state_machine/vru_role_state_machine.h"

namespace vanetza_integration
{
namespace vru
{
namespace role
{

const VruRoleSnapshot& VruRoleStateMachine::state() const noexcept
{
    return state_;
}

VruTransitionResult VruRoleStateMachine::activate() noexcept
{
    if (state_.role == VruRoleStatus::On)
    {
        return VruTransitionResult::NoChange;
    }

    state_.role = VruRoleStatus::On;
    state_.vbs = VbsState::Idle;
    return VruTransitionResult::Applied;
}

VruTransitionResult VruRoleStateMachine::deactivate() noexcept
{
    if (state_.role == VruRoleStatus::Off &&
        state_.vbs == VbsState::Idle)
    {
        return VruTransitionResult::NoChange;
    }

    state_.role = VruRoleStatus::Off;
    state_.vbs = VbsState::Idle;
    return VruTransitionResult::Applied;
}

VruTransitionResult VruRoleStateMachine::setVbsState(
    VbsState requestedState) noexcept
{
    if (state_.role == VruRoleStatus::Off)
    {
        return requestedState == VbsState::Idle &&
                       state_.vbs == VbsState::Idle
                   ? VruTransitionResult::NoChange
                   : VruTransitionResult::Rejected;
    }

    if (state_.vbs == requestedState)
    {
        return VruTransitionResult::NoChange;
    }

    state_.vbs = requestedState;
    return VruTransitionResult::Applied;
}

} // namespace role
} // namespace vru
} // namespace vanetza_integration
