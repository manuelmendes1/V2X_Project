#pragma once

#include <optional>
#include <string>
#include <variant>

#include "vanetza_integration/vam/types/vam_domain_types.h"
#include "vanetza_integration/vru/role/types/vru_role_types.h"

namespace vanetza_integration
{
namespace vam
{

enum class VruClusterLifecyclePhase
{
    None,
    Joining,
    MemberPendingConfirmation,
    Member,
    Leader,
    LeaderBreakupWarning,
    LeavingNotification,
    PassiveLowRisk,
    PassiveCpmCluster
};

// Contexto de clustering persistido pelo chamador.
// O estado VBS continua a pertencer a VruRoleStateMachine.
struct VruClusterLifecycleContext
{
    VruClusterLifecyclePhase phase{VruClusterLifecyclePhase::None};
    std::optional<VamClusterId> clusterId;
    std::optional<VamClusterOperationTime> joinTime;
    std::optional<VamClusterBreakupInfo> breakupInfo;
    std::optional<VamClusterLeaveInfo> leaveInfo;
    std::optional<VamClusterOperationTime> clusterIdChangeTime;

    friend bool operator==(
        const VruClusterLifecycleContext&,
        const VruClusterLifecycleContext&) = default;
};

// Os períodos de notificação são controlados externamente.
// Os eventos Complete* só devem ser enviados após os períodos
// normativos correspondentes.

struct EnterActiveStandalone
{
};

struct CreateCluster
{
    VamClusterId clusterId;
};

struct BeginJoin
{
    VamClusterId clusterId;
    VamClusterOperationTime joinTime;
};

struct RepeatJoinNotification
{
};

struct CompleteJoin
{
};

struct ConfirmJoin
{
};

struct CancelJoin
{
};

struct FailJoin
{
};

struct RepeatLeaveNotification
{
};

struct CompleteLeaveNotification
{
};

struct LeaveCluster
{
    VamClusterLeaveReason reason{
        VamClusterLeaveReason::NotProvided};
};

struct LeaderLost
{
};

struct BeginLeaderBreakup
{
    VamClusterBreakupInfo info;
    bool clusterRepresentedInCpm{false};
};

struct RepeatLeaderBreakupNotification
{
};

struct CompleteLeaderBreakup
{
};

struct ReceiveClusterBreakup
{
    VamClusterId clusterId;
    VamClusterBreakupInfo info;
    bool clusterRepresentedInCpm{false};
};

struct EnterLowRiskArea
{
};

struct ExitLowRiskArea
{
};

struct AnnounceClusterIdChange
{
    VamClusterOperationTime changeTime;
};

struct CompleteLeaderClusterIdChange
{
    VamClusterId newClusterId;
};

struct ReceiveClusterIdChangeNotice
{
    VamClusterOperationTime changeTime;
};

struct ResolveMemberClusterIdChange
{
    VamClusterId newClusterId;
    bool matchingCluster{false};
    VamClusterId leaveNotificationClusterId{};
    VamClusterLeaveReason leaveReason{
        VamClusterLeaveReason::NotProvided};
};

using VruClusterLifecycleEvent = std::variant<
    EnterActiveStandalone,
    CreateCluster,
    BeginJoin,
    RepeatJoinNotification,
    CompleteJoin,
    ConfirmJoin,
    CancelJoin,
    FailJoin,
    RepeatLeaveNotification,
    CompleteLeaveNotification,
    LeaveCluster,
    LeaderLost,
    BeginLeaderBreakup,
    RepeatLeaderBreakupNotification,
    CompleteLeaderBreakup,
    ReceiveClusterBreakup,
    EnterLowRiskArea,
    ExitLowRiskArea,
    AnnounceClusterIdChange,
    CompleteLeaderClusterIdChange,
    ReceiveClusterIdChangeNotice,
    ResolveMemberClusterIdChange>;

struct VruClusterLifecycleResult
{
    vru::role::VruTransitionResult transition{
        vru::role::VruTransitionResult::Rejected};

    vru::role::VruRoleSnapshot nextRole{};

    VruClusterLifecycleContext nextContext{};

    std::optional<VamClusterOperation> operation;

    std::string error;
};

class VruClusterLifecycleStateMachine
{
public:
    static VruClusterLifecycleResult Apply(
        const vru::role::VruRoleSnapshot& currentRole,
        const VruClusterLifecycleContext& currentContext,
        const VruClusterLifecycleEvent& event);
};

} // namespace vam
} // namespace vanetza_integration
