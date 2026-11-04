#pragma once

#include "vanetza_integration/vru/role/types/vru_role_types.h"

namespace vanetza_integration
{
namespace vru
{
namespace role
{

class VruRoleStateMachine
{
public:
    VruRoleStateMachine() = default;

    const VruRoleSnapshot& state() const noexcept;

    VruTransitionResult activate() noexcept;
    VruTransitionResult deactivate() noexcept;
    VruTransitionResult setVbsState(VbsState state) noexcept;

private:
    VruRoleSnapshot state_{};
};

} // namespace role
} // namespace vru
} // namespace vanetza_integration
