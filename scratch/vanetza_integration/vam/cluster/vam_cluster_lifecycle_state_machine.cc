#include "vanetza_integration/vam/cluster/vam_cluster_lifecycle_state_machine.h"

#include <type_traits>
#include <utility>

#include "vanetza_integration/vam/cluster/vam_cluster_operation_validation.h"

namespace vanetza_integration
{
namespace vam
{
namespace
{

using vru::role::VbsState;
using vru::role::VruRoleSnapshot;
using vru::role::VruRoleStatus;
using vru::role::VruTransitionResult;

using LifecycleContext = VruClusterLifecycleContext;
using LifecycleResult = VruClusterLifecycleResult;
using LifecyclePhase = VruClusterLifecyclePhase;

LifecycleResult MakeResult(
    VruTransitionResult transition,
    const VruRoleSnapshot& role,
    const LifecycleContext& context,
    std::optional<VamClusterOperation> operation = std::nullopt,
    std::string error = {})
{
    return {
        transition,
        role,
        context,
        std::move(operation),
        std::move(error)
    };
}

LifecycleResult Reject(
    const VruRoleSnapshot& role,
    const LifecycleContext& context,
    const char* reason)
{
    return MakeResult(
        VruTransitionResult::Rejected,
        role,
        context,
        std::nullopt,
        reason);
}

bool IsValidContext(const LifecycleContext& context)
{
    const bool hasId = context.clusterId.has_value();
    const bool hasJoinTime = context.joinTime.has_value();
    const bool hasBreakupInfo = context.breakupInfo.has_value();
    const bool hasLeaveInfo = context.leaveInfo.has_value();
    const bool hasIdChangeTime = context.clusterIdChangeTime.has_value();

    switch (context.phase)
    {
        case LifecyclePhase::None:
        case LifecyclePhase::PassiveLowRisk:
            return !hasId &&
                   !hasJoinTime &&
                   !hasBreakupInfo &&
                   !hasLeaveInfo &&
                   !hasIdChangeTime;

        case LifecyclePhase::Joining:
            return hasId &&
                   hasJoinTime &&
                   !hasBreakupInfo &&
                   !hasLeaveInfo &&
                   !hasIdChangeTime;

        case LifecyclePhase::MemberPendingConfirmation:
        case LifecyclePhase::PassiveCpmCluster:
            return hasId &&
                   !hasJoinTime &&
                   !hasBreakupInfo &&
                   !hasLeaveInfo &&
                   !hasIdChangeTime;

        case LifecyclePhase::Member:
        case LifecyclePhase::Leader:
            return hasId &&
                   !hasJoinTime &&
                   !hasBreakupInfo &&
                   !hasLeaveInfo;

        case LifecyclePhase::LeaderBreakupWarning:
            return hasId &&
                   !hasJoinTime &&
                   hasBreakupInfo &&
                   !hasLeaveInfo &&
                   !hasIdChangeTime;

        case LifecyclePhase::LeavingNotification:
            return !hasId &&
                   !hasJoinTime &&
                   !hasBreakupInfo &&
                   hasLeaveInfo &&
                   !hasIdChangeTime;
    }

    return false;
}

bool IsCompatibleRole(
    const VruRoleSnapshot& role,
    const LifecycleContext& context)
{
    if (role.role == VruRoleStatus::Off)
    {
        return role.vbs == VbsState::Idle &&
               context.phase == LifecyclePhase::None;
    }

    switch (context.phase)
    {
        case LifecyclePhase::None:
            return role.vbs == VbsState::Idle ||
                   role.vbs == VbsState::ActiveStandalone;

        case LifecyclePhase::Joining:
            return role.vbs == VbsState::ActiveStandalone;

        case LifecyclePhase::MemberPendingConfirmation:
        case LifecyclePhase::Member:
        case LifecyclePhase::PassiveLowRisk:
        case LifecyclePhase::PassiveCpmCluster:
            return role.vbs == VbsState::Passive;

        case LifecyclePhase::Leader:
        case LifecyclePhase::LeaderBreakupWarning:
            return role.vbs == VbsState::ActiveClusterLeader;

        case LifecyclePhase::LeavingNotification:
            return role.vbs == VbsState::ActiveStandalone;
    }

    return false;
}

bool ValidateOperation(
    const VamClusterOperation& operation,
    std::string& error)
{
    return VamClusterOperationValidation::Validate(operation, error);
}

bool ValidateContextPayloads(
    const LifecycleContext& context,
    std::string& error)
{
    if (context.joinTime.has_value())
    {
        VamClusterOperation operation;
        operation.clusterJoinInfo = VamClusterJoinInfo{
            *context.clusterId,
            *context.joinTime
        };

        if (!ValidateOperation(operation, error))
        {
            error = "invalid persisted join payload: " + error;
            return false;
        }
    }

    if (context.breakupInfo.has_value())
    {
        VamClusterOperation operation;
        operation.clusterBreakupInfo = *context.breakupInfo;

        if (!ValidateOperation(operation, error))
        {
            error = "invalid persisted breakup payload: " + error;
            return false;
        }
    }

    if (context.leaveInfo.has_value())
    {
        VamClusterOperation operation;
        operation.clusterLeaveInfo = *context.leaveInfo;

        if (!ValidateOperation(operation, error))
        {
            error = "invalid persisted leave payload: " + error;
            return false;
        }
    }

    if (context.clusterIdChangeTime.has_value())
    {
        VamClusterOperation operation;
        operation.clusterIdChangeTimeInfo =
            *context.clusterIdChangeTime;

        if (!ValidateOperation(operation, error))
        {
            error = "invalid persisted cluster ID change payload: " + error;
            return false;
        }
    }

    return true;
}

bool IsCpmBreakup(const VamClusterBreakupInfo& info)
{
    return info.clusterBreakupReason ==
        VamClusterBreakupReason::ReceptionOfCpmContainingCluster;
}

VamClusterOperation MakeJoinOperation(
    VamClusterId clusterId,
    const VamClusterOperationTime& joinTime)
{
    VamClusterOperation operation;
    operation.clusterJoinInfo = VamClusterJoinInfo{
        clusterId,
        joinTime
    };
    return operation;
}

VamClusterOperation MakeLeaveOperation(
    VamClusterId clusterId,
    VamClusterLeaveReason reason)
{
    VamClusterOperation operation;
    operation.clusterLeaveInfo = VamClusterLeaveInfo{
        clusterId,
        reason
    };
    return operation;
}

VamClusterOperation MakeLeaveOperation(
    const VamClusterLeaveInfo& info)
{
    return MakeLeaveOperation(info.clusterId, info.clusterLeaveReason);
}

VamClusterOperation MakeBreakupOperation(
    const VamClusterBreakupInfo& info)
{
    VamClusterOperation operation;
    operation.clusterBreakupInfo = info;
    return operation;
}

VamClusterOperation MakeIdChangeOperation(
    const VamClusterOperationTime& changeTime)
{
    VamClusterOperation operation;
    operation.clusterIdChangeTimeInfo = changeTime;
    return operation;
}

LifecycleContext MakeLeavingContext(
    VamClusterId clusterId,
    VamClusterLeaveReason reason)
{
    LifecycleContext context;
    context.phase = LifecyclePhase::LeavingNotification;
    context.leaveInfo = VamClusterLeaveInfo{clusterId, reason};
    return context;
}

} // namespace

LifecycleResult
VruClusterLifecycleStateMachine::Apply(
    const VruRoleSnapshot& currentRole,
    const LifecycleContext& currentContext,
    const VruClusterLifecycleEvent& event)
{
    if (!IsValidContext(currentContext))
    {
        return Reject(
            currentRole,
            currentContext,
            "cluster lifecycle context is inconsistent");
    }

    std::string payloadError;
    if (!ValidateContextPayloads(currentContext, payloadError))
    {
        return Reject(
            currentRole,
            currentContext,
            payloadError.c_str());
    }

    if (!IsCompatibleRole(currentRole, currentContext))
    {
        return Reject(
            currentRole,
            currentContext,
            "VBS role and cluster lifecycle phase are inconsistent");
    }

    if (currentRole.role != VruRoleStatus::On)
    {
        return Reject(
            currentRole,
            currentContext,
            "VRU role must be ON for cluster lifecycle events");
    }

    return std::visit(
        [&](const auto& value) -> LifecycleResult
        {
            using Event = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Event, EnterActiveStandalone>)
            {
                if (currentContext.phase != LifecyclePhase::None)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "standalone activation requires no cluster context");
                }

                if (currentRole.vbs == VbsState::ActiveStandalone)
                {
                    return MakeResult(
                        VruTransitionResult::NoChange,
                        currentRole,
                        currentContext);
                }

                if (currentRole.vbs != VbsState::Idle)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "standalone activation requires VBS Idle");
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    currentContext);
            }
            else if constexpr (std::is_same_v<Event, CreateCluster>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase != LifecyclePhase::None)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "cluster creation requires ActiveStandalone");
                }

                if (value.clusterId.value == 0)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "a newly created cluster ID must be non-zero");
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveClusterLeader;

                LifecycleContext nextContext;
                nextContext.phase = LifecyclePhase::Leader;
                nextContext.clusterId = value.clusterId;

                // ClusterInformation remains deliberately unmodelled.
                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext);
            }
            else if constexpr (std::is_same_v<Event, BeginJoin>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase != LifecyclePhase::None)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "join initiation requires ActiveStandalone");
                }

                auto operation = MakeJoinOperation(
                    value.clusterId,
                    value.joinTime);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                LifecycleContext nextContext;
                nextContext.phase = LifecyclePhase::Joining;
                nextContext.clusterId = value.clusterId;
                nextContext.joinTime = value.joinTime;

                return MakeResult(
                    VruTransitionResult::Applied,
                    currentRole,
                    nextContext,
                    std::move(operation));
            }
            else if constexpr (
                std::is_same_v<Event, RepeatJoinNotification>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase != LifecyclePhase::Joining ||
                    !currentContext.clusterId.has_value() ||
                    !currentContext.joinTime.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "join notification repetition requires Joining");
                }

                auto operation = MakeJoinOperation(
                    *currentContext.clusterId,
                    *currentContext.joinTime);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                return MakeResult(
                    VruTransitionResult::NoChange,
                    currentRole,
                    currentContext,
                    std::move(operation));
            }
            else if constexpr (std::is_same_v<Event, CompleteJoin>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase != LifecyclePhase::Joining ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "join notification completion requires Joining");
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::Passive;

                LifecycleContext nextContext;
                nextContext.phase =
                    LifecyclePhase::MemberPendingConfirmation;
                nextContext.clusterId = currentContext.clusterId;

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext);
            }
            else if constexpr (std::is_same_v<Event, ConfirmJoin>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    currentContext.phase !=
                        LifecyclePhase::MemberPendingConfirmation ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "join confirmation requires pending membership");
                }

                LifecycleContext nextContext;
                nextContext.phase = LifecyclePhase::Member;
                nextContext.clusterId = currentContext.clusterId;

                return MakeResult(
                    VruTransitionResult::Applied,
                    currentRole,
                    nextContext);
            }
            else if constexpr (std::is_same_v<Event, CancelJoin>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase != LifecyclePhase::Joining ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "join cancellation requires Joining");
                }

                constexpr auto reason =
                    VamClusterLeaveReason::CancelledJoin;

                auto operation = MakeLeaveOperation(
                    *currentContext.clusterId,
                    reason);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                auto nextContext = MakeLeavingContext(
                    *currentContext.clusterId,
                    reason);

                return MakeResult(
                    VruTransitionResult::Applied,
                    currentRole,
                    nextContext,
                    std::move(operation));
            }
            else if constexpr (std::is_same_v<Event, FailJoin>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    currentContext.phase !=
                        LifecyclePhase::MemberPendingConfirmation ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "join failure requires pending membership confirmation");
                }

                constexpr auto reason =
                    VamClusterLeaveReason::FailedJoin;

                auto operation = MakeLeaveOperation(
                    *currentContext.clusterId,
                    reason);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                auto nextContext = MakeLeavingContext(
                    *currentContext.clusterId,
                    reason);

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext,
                    std::move(operation));
            }
            else if constexpr (
                std::is_same_v<Event, RepeatLeaveNotification>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase !=
                        LifecyclePhase::LeavingNotification ||
                    !currentContext.leaveInfo.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "leave notification repetition requires LeavingNotification");
                }

                auto operation = MakeLeaveOperation(
                    *currentContext.leaveInfo);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                return MakeResult(
                    VruTransitionResult::NoChange,
                    currentRole,
                    currentContext,
                    std::move(operation));
            }
            else if constexpr (
                std::is_same_v<Event, CompleteLeaveNotification>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase !=
                        LifecyclePhase::LeavingNotification)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "leave notification completion requires LeavingNotification");
                }

                LifecycleContext nextContext;

                return MakeResult(
                    VruTransitionResult::Applied,
                    currentRole,
                    nextContext);
            }
            else if constexpr (std::is_same_v<Event, LeaveCluster>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    (currentContext.phase != LifecyclePhase::Member &&
                     currentContext.phase !=
                         LifecyclePhase::PassiveCpmCluster) ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "cluster leave requires a Passive cluster member");
                }

                auto operation = MakeLeaveOperation(
                    *currentContext.clusterId,
                    value.reason);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                auto nextContext = MakeLeavingContext(
                    *currentContext.clusterId,
                    value.reason);

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext,
                    std::move(operation));
            }
            else if constexpr (std::is_same_v<Event, LeaderLost>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    currentContext.phase != LifecyclePhase::Member ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "leader loss requires a Passive cluster member");
                }

                constexpr auto reason =
                    VamClusterLeaveReason::ClusterLeaderLost;

                auto operation = MakeLeaveOperation(
                    *currentContext.clusterId,
                    reason);

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                auto nextContext = MakeLeavingContext(
                    *currentContext.clusterId,
                    reason);

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext,
                    std::move(operation));
            }
            else if constexpr (std::is_same_v<Event, BeginLeaderBreakup>)
            {
                if (currentRole.vbs != VbsState::ActiveClusterLeader ||
                    currentContext.phase != LifecyclePhase::Leader ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "leader breakup requires the Leader phase");
                }

                if (value.clusterRepresentedInCpm != IsCpmBreakup(value.info))
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "CPM breakup flag and breakup reason disagree");
                }

                auto operation = MakeBreakupOperation(value.info);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                LifecycleContext nextContext;
                nextContext.clusterId = currentContext.clusterId;

                auto nextRole = currentRole;

                if (value.clusterRepresentedInCpm)
                {
                    // Normative CPM exception: the leader may move
                    // directly to Passive.
                    nextRole.vbs = VbsState::Passive;
                    nextContext.phase = LifecyclePhase::PassiveCpmCluster;
                }
                else
                {
                    nextContext.phase =
                        LifecyclePhase::LeaderBreakupWarning;
                    nextContext.breakupInfo = value.info;
                }

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext,
                    std::move(operation));
            }
            else if constexpr (
                std::is_same_v<Event, RepeatLeaderBreakupNotification>)
            {
                if (currentRole.vbs != VbsState::ActiveClusterLeader ||
                    currentContext.phase !=
                        LifecyclePhase::LeaderBreakupWarning ||
                    !currentContext.breakupInfo.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "breakup notification repetition requires warning phase");
                }

                auto operation = MakeBreakupOperation(
                    *currentContext.breakupInfo);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                return MakeResult(
                    VruTransitionResult::NoChange,
                    currentRole,
                    currentContext,
                    std::move(operation));
            }
            else if constexpr (
                std::is_same_v<Event, CompleteLeaderBreakup>)
            {
                if (currentRole.vbs != VbsState::ActiveClusterLeader ||
                    currentContext.phase !=
                        LifecyclePhase::LeaderBreakupWarning)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "leader breakup completion requires warning phase");
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                LifecycleContext nextContext;

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext);
            }
            else if constexpr (std::is_same_v<Event, ReceiveClusterBreakup>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    currentContext.phase != LifecyclePhase::Member ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "received breakup requires a Passive cluster member");
                }

                if (value.clusterId != *currentContext.clusterId)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "received breakup cluster ID does not match membership");
                }

                if (value.clusterRepresentedInCpm != IsCpmBreakup(value.info))
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "CPM breakup flag and breakup reason disagree");
                }

                auto breakupOperation = MakeBreakupOperation(value.info);

                std::string error;
                if (!ValidateOperation(breakupOperation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                if (value.clusterRepresentedInCpm)
                {
                    LifecycleContext nextContext;
                    nextContext.phase =
                        LifecyclePhase::PassiveCpmCluster;
                    nextContext.clusterId = currentContext.clusterId;

                    // The cluster is already represented by CPM.
                    // The member may remain Passive.
                    return MakeResult(
                        VruTransitionResult::Applied,
                        currentRole,
                        nextContext);
                }

                constexpr auto leaveReason =
                    VamClusterLeaveReason::ClusterDisbandedByLeader;

                auto leaveOperation = MakeLeaveOperation(
                    *currentContext.clusterId,
                    leaveReason);

                if (!ValidateOperation(leaveOperation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                auto nextContext = MakeLeavingContext(
                    *currentContext.clusterId,
                    leaveReason);

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext,
                    std::move(leaveOperation));
            }
            else if constexpr (std::is_same_v<Event, EnterLowRiskArea>)
            {
                if (currentRole.vbs != VbsState::ActiveStandalone ||
                    currentContext.phase != LifecyclePhase::None)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "low-risk entry requires ActiveStandalone");
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::Passive;

                LifecycleContext nextContext;
                nextContext.phase = LifecyclePhase::PassiveLowRisk;

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext);
            }
            else if constexpr (std::is_same_v<Event, ExitLowRiskArea>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    currentContext.phase != LifecyclePhase::PassiveLowRisk)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "low-risk exit requires PassiveLowRisk");
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                LifecycleContext nextContext;

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext);
            }
            else if constexpr (
                std::is_same_v<Event, AnnounceClusterIdChange>)
            {
                if (currentRole.vbs != VbsState::ActiveClusterLeader ||
                    currentContext.phase != LifecyclePhase::Leader ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "cluster ID change announcement requires Leader");
                }

                auto operation = MakeIdChangeOperation(value.changeTime);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                if (currentContext.clusterIdChangeTime.has_value() &&
                    *currentContext.clusterIdChangeTime != value.changeTime)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "a different cluster ID change is already pending");
                }

                auto nextContext = currentContext;
                nextContext.clusterIdChangeTime = value.changeTime;

                // The announcement carries the change time, not the new ID.
                // Repeated announcements reproduce the same VAM operation.
                return MakeResult(
                    VruTransitionResult::NoChange,
                    currentRole,
                    nextContext,
                    std::move(operation));
            }
            else if constexpr (
                std::is_same_v<Event, CompleteLeaderClusterIdChange>)
            {
                if (currentRole.vbs != VbsState::ActiveClusterLeader ||
                    currentContext.phase != LifecyclePhase::Leader ||
                    !currentContext.clusterId.has_value() ||
                    !currentContext.clusterIdChangeTime.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "leader ID change completion requires a pending announcement");
                }

                if (value.newClusterId.value == 0 ||
                    value.newClusterId == *currentContext.clusterId)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "new leader cluster ID must be non-zero and different");
                }

                auto nextContext = currentContext;
                nextContext.clusterId = value.newClusterId;
                nextContext.clusterIdChangeTime.reset();

                return MakeResult(
                    VruTransitionResult::Applied,
                    currentRole,
                    nextContext);
            }
            else if constexpr (
                std::is_same_v<Event, ReceiveClusterIdChangeNotice>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    currentContext.phase != LifecyclePhase::Member ||
                    !currentContext.clusterId.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "cluster ID change notice requires a Passive member");
                }

                auto operation = MakeIdChangeOperation(value.changeTime);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                if (currentContext.clusterIdChangeTime.has_value())
                {
                    if (*currentContext.clusterIdChangeTime != value.changeTime)
                    {
                        return Reject(
                            currentRole,
                            currentContext,
                            "a different cluster ID change notice is already pending");
                    }

                    return MakeResult(
                        VruTransitionResult::NoChange,
                        currentRole,
                        currentContext);
                }

                auto nextContext = currentContext;
                nextContext.clusterIdChangeTime = value.changeTime;

                // The member records the notice but does not retransmit it.
                return MakeResult(
                    VruTransitionResult::NoChange,
                    currentRole,
                    nextContext);
            }
            else if constexpr (
                std::is_same_v<Event, ResolveMemberClusterIdChange>)
            {
                if (currentRole.vbs != VbsState::Passive ||
                    currentContext.phase != LifecyclePhase::Member ||
                    !currentContext.clusterId.has_value() ||
                    !currentContext.clusterIdChangeTime.has_value())
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "member ID change resolution requires a pending notice");
                }

                if (value.newClusterId.value == 0 ||
                    value.newClusterId == *currentContext.clusterId)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "observed new cluster ID must be non-zero and different");
                }

                if (value.matchingCluster)
                {
                    auto nextContext = currentContext;
                    nextContext.clusterId = value.newClusterId;
                    nextContext.clusterIdChangeTime.reset();

                    // Similarity of bounding boxes and dynamics is decided
                    // by the external cluster observation/policy component.
                    return MakeResult(
                        VruTransitionResult::Applied,
                        currentRole,
                        nextContext);
                }

                if (value.leaveNotificationClusterId !=
                        *currentContext.clusterId &&
                    value.leaveNotificationClusterId != value.newClusterId)
                {
                    return Reject(
                        currentRole,
                        currentContext,
                        "leave indication must use the old or new cluster ID");
                }

                auto operation = MakeLeaveOperation(
                    value.leaveNotificationClusterId,
                    value.leaveReason);

                std::string error;
                if (!ValidateOperation(operation, error))
                {
                    return Reject(currentRole, currentContext, error.c_str());
                }

                auto nextRole = currentRole;
                nextRole.vbs = VbsState::ActiveStandalone;

                auto nextContext = MakeLeavingContext(
                    value.leaveNotificationClusterId,
                    value.leaveReason);

                return MakeResult(
                    VruTransitionResult::Applied,
                    nextRole,
                    nextContext,
                    std::move(operation));
            }
            else
            {
                return Reject(
                    currentRole,
                    currentContext,
                    "unsupported cluster lifecycle event");
            }
        },
        event);
}

} // namespace vam
} // namespace vanetza_integration
