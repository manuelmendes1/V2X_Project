#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "vanetza_integration/vam/cluster/vam_cluster_lifecycle_state_machine.h"

using namespace vanetza_integration::vam;
using namespace vanetza_integration::vru::role;

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

void RequireRole(
    const VruClusterLifecycleResult& result,
    VruRoleStatus role,
    VbsState vbs,
    const char* message)
{
    Require(result.nextRole.role == role, message);
    Require(result.nextRole.vbs == vbs, message);
}

void RequireLeaveOperation(
    const VruClusterLifecycleResult& result,
    VamClusterId clusterId,
    VamClusterLeaveReason reason,
    const char* message)
{
    Require(result.operation.has_value(), message);
    Require(
        result.operation->clusterLeaveInfo.has_value(),
        message);
    Require(
        result.operation->clusterLeaveInfo->clusterId == clusterId,
        message);
    Require(
        result.operation->clusterLeaveInfo->clusterLeaveReason == reason,
        message);
}

VruClusterLifecycleContext MakeContext(
    VruClusterLifecyclePhase phase,
    VamClusterId clusterId)
{
    VruClusterLifecycleContext context;
    context.phase = phase;
    context.clusterId = clusterId;
    return context;
}

VruRoleSnapshot ActiveStandalone()
{
    return {VruRoleStatus::On, VbsState::ActiveStandalone};
}

VruRoleSnapshot Passive()
{
    return {VruRoleStatus::On, VbsState::Passive};
}

VruRoleSnapshot ClusterLeader()
{
    return {VruRoleStatus::On, VbsState::ActiveClusterLeader};
}

VamClusterOperationTime TimeMs(std::int64_t milliseconds)
{
    return {std::chrono::milliseconds{milliseconds}};
}

void TestStandaloneAndClusterCreation()
{
    const VruRoleSnapshot activeRole{
        VruRoleStatus::On,
        VbsState::Idle
    };

    const auto standalone = VruClusterLifecycleStateMachine::Apply(
        activeRole,
        {},
        EnterActiveStandalone{});

    Require(
        standalone.transition == VruTransitionResult::Applied,
        "ON/IDLE must be allowed to enter ACTIVE_STANDALONE");

    RequireRole(
        standalone,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "standalone transition must update VBS state");

    Require(
        standalone.nextContext.phase == VruClusterLifecyclePhase::None,
        "standalone transition must have no cluster membership");

    const auto created = VruClusterLifecycleStateMachine::Apply(
        ActiveStandalone(),
        {},
        CreateCluster{VamClusterId{7}});

    Require(
        created.transition == VruTransitionResult::Applied,
        "cluster creation must be accepted from ACTIVE_STANDALONE");

    RequireRole(
        created,
        VruRoleStatus::On,
        VbsState::ActiveClusterLeader,
        "cluster creation must produce ACTIVE_CLUSTER_LEADER");

    Require(
        created.nextContext.phase == VruClusterLifecyclePhase::Leader,
        "created cluster must be represented as Leader phase");

    Require(
        created.nextContext.clusterId == VamClusterId{7},
        "leader context must retain the allocated cluster ID");

    Require(
        !created.operation.has_value(),
        "cluster creation must not fabricate VAM cluster information");

    const auto zeroId = VruClusterLifecycleStateMachine::Apply(
        ActiveStandalone(),
        {},
        CreateCluster{VamClusterId{0}});

    Require(
        zeroId.transition == VruTransitionResult::Rejected,
        "a locally created cluster must reject ID zero");
}

void TestJoinLifecycle()
{
    const auto begin = VruClusterLifecycleStateMachine::Apply(
        ActiveStandalone(),
        {},
        BeginJoin{VamClusterId{0}, TimeMs(5000)});

    Require(
        begin.transition == VruTransitionResult::Applied,
        "join intent must be accepted from ACTIVE_STANDALONE");

    RequireRole(
        begin,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "join notification must not make the subject Passive immediately");

    Require(
        begin.nextContext.phase == VruClusterLifecyclePhase::Joining,
        "join intent must persist the Joining phase");

    Require(
        begin.nextContext.clusterId == VamClusterId{0},
        "join context must preserve cluster ID zero");

    Require(
        begin.operation.has_value() &&
            begin.operation->clusterJoinInfo.has_value(),
        "join intent must produce clusterJoinInfo");

    Require(
        begin.operation->clusterJoinInfo->clusterId == VamClusterId{0},
        "join operation must use the requested cluster ID");

    Require(
        begin.operation->clusterJoinInfo->joinTime == TimeMs(5000),
        "join operation must preserve the notification time");

    const auto repeat = VruClusterLifecycleStateMachine::Apply(
        begin.nextRole,
        begin.nextContext,
        RepeatJoinNotification{});

    Require(
        repeat.transition == VruTransitionResult::NoChange,
        "repeated join notification must not change lifecycle state");

    Require(
        repeat.operation == begin.operation,
        "repeated join notification must reproduce the same operation");

    const auto complete = VruClusterLifecycleStateMachine::Apply(
        begin.nextRole,
        begin.nextContext,
        CompleteJoin{});

    Require(
        complete.transition == VruTransitionResult::Applied,
        "completed join notification must allow transition to Passive");

    RequireRole(
        complete,
        VruRoleStatus::On,
        VbsState::Passive,
        "successful join must produce Passive");

    Require(
        complete.nextContext.phase ==
            VruClusterLifecyclePhase::MemberPendingConfirmation,
        "completed join notifications must await cluster confirmation");

    Require(
        complete.nextContext.clusterId == VamClusterId{0},
        "member context must preserve cluster ID zero");

    Require(
        !complete.nextContext.joinTime.has_value(),
        "completed join must clear pending join time");

    const auto confirmed = VruClusterLifecycleStateMachine::Apply(
        complete.nextRole,
        complete.nextContext,
        ConfirmJoin{});

    Require(
        confirmed.transition == VruTransitionResult::Applied,
        "observed cluster membership must confirm the pending join");

    RequireRole(
        confirmed,
        VruRoleStatus::On,
        VbsState::Passive,
        "confirmation must preserve Passive VBS state");

    Require(
        confirmed.nextContext.phase == VruClusterLifecyclePhase::Member,
        "confirmed join must produce Member phase");
}

void TestJoinCancellationAndFailure()
{
    const auto begin = VruClusterLifecycleStateMachine::Apply(
        ActiveStandalone(),
        {},
        BeginJoin{VamClusterId{12}, TimeMs(3000)});

    Require(
        begin.transition == VruTransitionResult::Applied,
        "join setup must succeed");

    const auto cancelled = VruClusterLifecycleStateMachine::Apply(
        begin.nextRole,
        begin.nextContext,
        CancelJoin{});

    Require(
        cancelled.transition == VruTransitionResult::Applied,
        "pending join must be cancellable");

    RequireRole(
        cancelled,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "cancelled join must remain ACTIVE_STANDALONE");

    Require(
        cancelled.nextContext.phase ==
            VruClusterLifecyclePhase::LeavingNotification,
        "cancelled join must retain its leave-notification context");

    RequireLeaveOperation(
        cancelled,
        VamClusterId{12},
        VamClusterLeaveReason::CancelledJoin,
        "cancelled join must emit the matching leave operation");

    const auto repeatedCancel =
        VruClusterLifecycleStateMachine::Apply(
            cancelled.nextRole,
            cancelled.nextContext,
            RepeatLeaveNotification{});

    Require(
        repeatedCancel.transition == VruTransitionResult::NoChange,
        "repeated leave notification must not change lifecycle state");

    RequireLeaveOperation(
        repeatedCancel,
        VamClusterId{12},
        VamClusterLeaveReason::CancelledJoin,
        "cancelled-join leave notification must be repeatable");

    const auto completedCancel =
        VruClusterLifecycleStateMachine::Apply(
            repeatedCancel.nextRole,
            repeatedCancel.nextContext,
            CompleteLeaveNotification{});

    Require(
        completedCancel.nextContext.phase == VruClusterLifecyclePhase::None,
        "completed leave notification must clear pending context");

    const auto pendingConfirmation =
        VruClusterLifecycleStateMachine::Apply(
            begin.nextRole,
            begin.nextContext,
            CompleteJoin{});

    const auto failed = VruClusterLifecycleStateMachine::Apply(
        pendingConfirmation.nextRole,
        pendingConfirmation.nextContext,
        FailJoin{});

    Require(
        failed.transition == VruTransitionResult::Applied,
        "pending join must support failure");

    RequireLeaveOperation(
        failed,
        VamClusterId{12},
        VamClusterLeaveReason::FailedJoin,
        "failed join must emit the matching leave operation");

    Require(
        failed.nextContext.phase ==
            VruClusterLifecyclePhase::LeavingNotification,
        "failed join must retain the leave-notification context");
}


void TestJoinEventsArePhaseSpecific()
{
    const auto begin = VruClusterLifecycleStateMachine::Apply(
        ActiveStandalone(),
        {},
        BeginJoin{VamClusterId{14}, TimeMs(2500)});

    Require(
        begin.transition == VruTransitionResult::Applied,
        "phase-specific join setup must succeed");

    const auto prematureFailure = VruClusterLifecycleStateMachine::Apply(
        begin.nextRole,
        begin.nextContext,
        FailJoin{});

    Require(
        prematureFailure.transition == VruTransitionResult::Rejected,
        "FailJoin must not be accepted during Joining");

    Require(
        prematureFailure.nextRole.vbs == begin.nextRole.vbs &&
            prematureFailure.nextContext == begin.nextContext,
        "premature FailJoin must preserve state and context");

    Require(
        !prematureFailure.operation.has_value(),
        "premature FailJoin must not produce an operation");

    const auto pending = VruClusterLifecycleStateMachine::Apply(
        begin.nextRole,
        begin.nextContext,
        CompleteJoin{});

    Require(
        pending.nextContext.phase ==
            VruClusterLifecyclePhase::MemberPendingConfirmation,
        "join notification completion must enter pending confirmation");

    const auto lateCancellation = VruClusterLifecycleStateMachine::Apply(
        pending.nextRole,
        pending.nextContext,
        CancelJoin{});

    Require(
        lateCancellation.transition == VruTransitionResult::Rejected,
        "CancelJoin must not be accepted after join notifications complete");

    Require(
        lateCancellation.nextRole.vbs == pending.nextRole.vbs &&
            lateCancellation.nextContext == pending.nextContext,
        "late CancelJoin must preserve state and context");

    Require(
        !lateCancellation.operation.has_value(),
        "late CancelJoin must not produce an operation");

    const auto failed = VruClusterLifecycleStateMachine::Apply(
        pending.nextRole,
        pending.nextContext,
        FailJoin{});

    Require(
        failed.transition == VruTransitionResult::Applied,
        "FailJoin must be accepted while awaiting membership confirmation");

    RequireRole(
        failed,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "failed confirmation must resume individual VAM state");

    RequireLeaveOperation(
        failed,
        VamClusterId{14},
        VamClusterLeaveReason::FailedJoin,
        "failed confirmation must announce FailedJoin");
}

void TestMemberLeaveAndLeaderLoss()
{
    const auto memberContext = MakeContext(
        VruClusterLifecyclePhase::Member,
        VamClusterId{21});

    const auto leave = VruClusterLifecycleStateMachine::Apply(
        Passive(),
        memberContext,
        LeaveCluster{VamClusterLeaveReason::OutOfClusterBoundingBox});

    Require(
        leave.transition == VruTransitionResult::Applied,
        "member must be able to leave its cluster");

    RequireRole(
        leave,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "normal member leave must return to ACTIVE_STANDALONE");

    RequireLeaveOperation(
        leave,
        VamClusterId{21},
        VamClusterLeaveReason::OutOfClusterBoundingBox,
        "member leave must preserve cluster ID and reason");

    Require(
        leave.nextContext.phase ==
            VruClusterLifecyclePhase::LeavingNotification,
        "member leave must retain notification context");

    const auto repeatedLeave = VruClusterLifecycleStateMachine::Apply(
        leave.nextRole,
        leave.nextContext,
        RepeatLeaveNotification{});

    Require(
        repeatedLeave.transition == VruTransitionResult::NoChange,
        "repeated member leave notification must be idempotent");

    RequireLeaveOperation(
        repeatedLeave,
        VamClusterId{21},
        VamClusterLeaveReason::OutOfClusterBoundingBox,
        "member leave operation must be repeatable");

    const auto completedLeave =
        VruClusterLifecycleStateMachine::Apply(
            repeatedLeave.nextRole,
            repeatedLeave.nextContext,
            CompleteLeaveNotification{});

    Require(
        completedLeave.nextContext.phase == VruClusterLifecyclePhase::None,
        "completed member leave notification must clear context");

    const auto leaderLost = VruClusterLifecycleStateMachine::Apply(
        Passive(),
        memberContext,
        LeaderLost{});

    Require(
        leaderLost.transition == VruTransitionResult::Applied,
        "member must be able to leave after leader loss");

    RequireRole(
        leaderLost,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "leader loss must return the member to ACTIVE_STANDALONE");

    RequireLeaveOperation(
        leaderLost,
        VamClusterId{21},
        VamClusterLeaveReason::ClusterLeaderLost,
        "leader loss must emit ClusterLeaderLost");

    Require(
        leaderLost.nextContext.phase ==
            VruClusterLifecyclePhase::LeavingNotification,
        "leader loss must retain leave-notification context");
}

void TestLeaderBreakupLifecycle()
{
    const auto leaderContext = MakeContext(
        VruClusterLifecyclePhase::Leader,
        VamClusterId{31});

    const VamClusterBreakupInfo info{
        VamClusterBreakupReason::ClusteringPurposeCompleted,
        TimeMs(3000)
    };

    const auto begin = VruClusterLifecycleStateMachine::Apply(
        ClusterLeader(),
        leaderContext,
        BeginLeaderBreakup{info, false});

    Require(
        begin.transition == VruTransitionResult::Applied,
        "leader must be able to begin cluster breakup");

    RequireRole(
        begin,
        VruRoleStatus::On,
        VbsState::ActiveClusterLeader,
        "leader must remain active during breakup warning");

    Require(
        begin.nextContext.phase ==
            VruClusterLifecyclePhase::LeaderBreakupWarning,
        "leader must retain the breakup warning phase");

    Require(
        begin.operation.has_value() &&
            begin.operation->clusterBreakupInfo == info,
        "leader breakup must produce clusterBreakupInfo");

    const auto repeat = VruClusterLifecycleStateMachine::Apply(
        begin.nextRole,
        begin.nextContext,
        RepeatLeaderBreakupNotification{});

    Require(
        repeat.transition == VruTransitionResult::NoChange,
        "repeated breakup notification must not change lifecycle state");

    Require(
        repeat.operation == begin.operation,
        "repeated breakup notification must reproduce the same operation");

    const auto complete = VruClusterLifecycleStateMachine::Apply(
        begin.nextRole,
        begin.nextContext,
        CompleteLeaderBreakup{});

    Require(
        complete.transition == VruTransitionResult::Applied,
        "leader must complete breakup after the warning interval");

    RequireRole(
        complete,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "completed leader breakup must return to ACTIVE_STANDALONE");

    Require(
        complete.nextContext.phase == VruClusterLifecyclePhase::None,
        "completed leader breakup must clear cluster context");
}

void TestMemberBreakupAndCpmException()
{
    const auto memberContext = MakeContext(
        VruClusterLifecyclePhase::Member,
        VamClusterId{41});

    const VamClusterBreakupInfo normalInfo{
        VamClusterBreakupReason::ClusteringPurposeCompleted,
        TimeMs(3000)
    };

    const auto received = VruClusterLifecycleStateMachine::Apply(
        Passive(),
        memberContext,
        ReceiveClusterBreakup{
            VamClusterId{41},
            normalInfo,
            false
        });

    Require(
        received.transition == VruTransitionResult::Applied,
        "member must accept a matching cluster breakup");

    RequireRole(
        received,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "receiving cluster breakup must resume individual VAMs");

    Require(
        received.nextContext.phase ==
            VruClusterLifecyclePhase::LeavingNotification,
        "received breakup must start leave notifications");

    RequireLeaveOperation(
        received,
        VamClusterId{41},
        VamClusterLeaveReason::ClusterDisbandedByLeader,
        "received breakup must announce ClusterDisbandedByLeader");

    const auto repeated = VruClusterLifecycleStateMachine::Apply(
        received.nextRole,
        received.nextContext,
        RepeatLeaveNotification{});

    Require(
        repeated.transition == VruTransitionResult::NoChange,
        "breakup leave notification repetition must be idempotent");

    RequireLeaveOperation(
        repeated,
        VamClusterId{41},
        VamClusterLeaveReason::ClusterDisbandedByLeader,
        "breakup leave notification must be repeatable");

    const auto completed = VruClusterLifecycleStateMachine::Apply(
        repeated.nextRole,
        repeated.nextContext,
        CompleteLeaveNotification{});

    Require(
        completed.nextContext.phase == VruClusterLifecyclePhase::None,
        "completed breakup leave notification must clear context");

    RequireRole(
        completed,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "completed breakup notification must preserve ActiveStandalone");

    const VamClusterBreakupInfo cpmInfo{
        VamClusterBreakupReason::ReceptionOfCpmContainingCluster,
        TimeMs(3000)
    };

    const auto cpmMember = VruClusterLifecycleStateMachine::Apply(
        Passive(),
        memberContext,
        ReceiveClusterBreakup{
            VamClusterId{41},
            cpmInfo,
            true
        });

    Require(
        cpmMember.transition == VruTransitionResult::Applied,
        "member must support the CPM breakup exception");

    RequireRole(
        cpmMember,
        VruRoleStatus::On,
        VbsState::Passive,
        "CPM-represented member may remain Passive");

    Require(
        cpmMember.nextContext.phase ==
            VruClusterLifecyclePhase::PassiveCpmCluster,
        "CPM exception must be distinguishable from normal membership");

    const auto cpmLeader = VruClusterLifecycleStateMachine::Apply(
        ClusterLeader(),
        MakeContext(
            VruClusterLifecyclePhase::Leader,
            VamClusterId{42}),
        BeginLeaderBreakup{cpmInfo, true});

    Require(
        cpmLeader.transition == VruTransitionResult::Applied,
        "leader must support direct Passive transition for CPM exception");

    RequireRole(
        cpmLeader,
        VruRoleStatus::On,
        VbsState::Passive,
        "CPM exception may move leader directly to Passive");

    Require(
        cpmLeader.nextContext.phase ==
            VruClusterLifecyclePhase::PassiveCpmCluster,
        "CPM leader must retain distinct passive context");
}

void TestLowRiskAndClusterIdChange()
{
    const auto enterLowRisk = VruClusterLifecycleStateMachine::Apply(
        ActiveStandalone(),
        {},
        EnterLowRiskArea{});

    Require(
        enterLowRisk.transition == VruTransitionResult::Applied,
        "standalone VRU must be able to enter low-risk Passive state");

    RequireRole(
        enterLowRisk,
        VruRoleStatus::On,
        VbsState::Passive,
        "low-risk entry must produce Passive");

    Require(
        enterLowRisk.nextContext.phase ==
            VruClusterLifecyclePhase::PassiveLowRisk,
        "low-risk Passive must not be confused with cluster membership");

    const auto exitLowRisk = VruClusterLifecycleStateMachine::Apply(
        enterLowRisk.nextRole,
        enterLowRisk.nextContext,
        ExitLowRiskArea{});

    Require(
        exitLowRisk.transition == VruTransitionResult::Applied,
        "VRU must be able to leave low-risk Passive state");

    RequireRole(
        exitLowRisk,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "leaving low-risk area must return to ACTIVE_STANDALONE");

    const auto idChange = VruClusterLifecycleStateMachine::Apply(
        ClusterLeader(),
        MakeContext(
            VruClusterLifecyclePhase::Leader,
            VamClusterId{51}),
        AnnounceClusterIdChange{TimeMs(5000)});

    Require(
        idChange.transition == VruTransitionResult::NoChange,
        "ID change announcement must not itself change VBS state");

    RequireRole(
        idChange,
        VruRoleStatus::On,
        VbsState::ActiveClusterLeader,
        "ID change announcement must preserve leader state");

    Require(
        idChange.operation.has_value() &&
            idChange.operation->clusterIdChangeTimeInfo.has_value(),
        "ID change announcement must produce clusterIdChangeTimeInfo");

    Require(
        *idChange.operation->clusterIdChangeTimeInfo == TimeMs(5000),
        "ID change operation must preserve the requested time");
}


void TestClusterIdChangeMemberLifecycle()
{
    const VamClusterOperationTime changeTime = TimeMs(3000);

    // Leader advertises the planned change and retains its pending deadline.
    const auto leaderContext = MakeContext(
        VruClusterLifecyclePhase::Leader,
        VamClusterId{71});

    const auto announce = VruClusterLifecycleStateMachine::Apply(
        ClusterLeader(),
        leaderContext,
        AnnounceClusterIdChange{changeTime});

    Require(
        announce.transition == VruTransitionResult::NoChange,
        "leader ID change announcement must not alter VBS state");

    Require(
        announce.operation.has_value() &&
            announce.operation->clusterIdChangeTimeInfo == changeTime,
        "leader announcement must produce clusterIdChangeTimeInfo");

    Require(
        announce.nextContext.clusterIdChangeTime == changeTime,
        "leader must persist the pending ID change time");

    const auto repeatedAnnouncement =
        VruClusterLifecycleStateMachine::Apply(
            announce.nextRole,
            announce.nextContext,
            AnnounceClusterIdChange{changeTime});

    Require(
        repeatedAnnouncement.transition == VruTransitionResult::NoChange,
        "repeated leader announcement must be idempotent");

    Require(
        repeatedAnnouncement.operation == announce.operation,
        "repeated leader announcement must preserve operation content");

    const auto leaderChanged = VruClusterLifecycleStateMachine::Apply(
        announce.nextRole,
        announce.nextContext,
        CompleteLeaderClusterIdChange{VamClusterId{72}});

    Require(
        leaderChanged.transition == VruTransitionResult::Applied,
        "leader must apply the new cluster ID after notification");

    RequireRole(
        leaderChanged,
        VruRoleStatus::On,
        VbsState::ActiveClusterLeader,
        "leader ID change must preserve ActiveClusterLeader");

    Require(
        leaderChanged.nextContext.clusterId == VamClusterId{72},
        "leader context must retain the new cluster ID");

    Require(
        !leaderChanged.nextContext.clusterIdChangeTime.has_value(),
        "leader ID change completion must clear pending time");

    // A member records the incoming announcement while remaining Passive.
    const auto memberContext = MakeContext(
        VruClusterLifecyclePhase::Member,
        VamClusterId{81});

    const auto notice = VruClusterLifecycleStateMachine::Apply(
        Passive(),
        memberContext,
        ReceiveClusterIdChangeNotice{changeTime});

    Require(
        notice.transition == VruTransitionResult::NoChange,
        "member ID change notice must not alter VBS state");

    RequireRole(
        notice,
        VruRoleStatus::On,
        VbsState::Passive,
        "member must remain Passive while awaiting the ID change");

    Require(
        notice.nextContext.clusterIdChangeTime == changeTime,
        "member must persist the pending ID change time");

    const auto repeatedNotice = VruClusterLifecycleStateMachine::Apply(
        notice.nextRole,
        notice.nextContext,
        ReceiveClusterIdChangeNotice{changeTime});

    Require(
        repeatedNotice.transition == VruTransitionResult::NoChange,
        "repeated member notice must be idempotent");

    // The external observer confirms that the new ID describes the same
    // cluster by comparing bounding boxes and dynamic properties.
    const auto matched = VruClusterLifecycleStateMachine::Apply(
        notice.nextRole,
        notice.nextContext,
        ResolveMemberClusterIdChange{
            VamClusterId{82},
            true,
            VamClusterId{81},
            VamClusterLeaveReason::NotProvided
        });

    Require(
        matched.transition == VruTransitionResult::Applied,
        "matching new cluster must update member context");

    RequireRole(
        matched,
        VruRoleStatus::On,
        VbsState::Passive,
        "matching ID change must preserve Passive state");

    Require(
        matched.nextContext.phase == VruClusterLifecyclePhase::Member &&
            matched.nextContext.clusterId == VamClusterId{82},
        "member must update its cluster ID after a successful match");

    Require(
        !matched.nextContext.clusterIdChangeTime.has_value(),
        "successful member ID update must clear pending time");

    // If no equivalent cluster is found, the member leaves the old cluster.
    const auto noMatch = VruClusterLifecycleStateMachine::Apply(
        notice.nextRole,
        notice.nextContext,
        ResolveMemberClusterIdChange{
            VamClusterId{82},
            false,
            VamClusterId{81},
            VamClusterLeaveReason::SafetyCondition
        });

    Require(
        noMatch.transition == VruTransitionResult::Applied,
        "member must leave when no equivalent new cluster is found");

    RequireRole(
        noMatch,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "unmatched ID change must resume individual VAMs");

    Require(
        noMatch.nextContext.phase ==
            VruClusterLifecyclePhase::LeavingNotification,
        "unmatched ID change must start leave notifications");

    RequireLeaveOperation(
        noMatch,
        VamClusterId{81},
        VamClusterLeaveReason::SafetyCondition,
        "member may use the old cluster ID during ID persistence");

    // ETSI also permits the new ID in the leave indication during
    // timeClusterIdPersist. The caller selects the permitted ID.
    const auto leaveWithNewId =
        VruClusterLifecycleStateMachine::Apply(
            notice.nextRole,
            notice.nextContext,
            ResolveMemberClusterIdChange{
                VamClusterId{82},
                false,
                VamClusterId{82},
                VamClusterLeaveReason::SafetyCondition
            });

    RequireLeaveOperation(
        leaveWithNewId,
        VamClusterId{82},
        VamClusterLeaveReason::SafetyCondition,
        "member may also use the new cluster ID during ID persistence");

    const auto invalidLeaveId =
        VruClusterLifecycleStateMachine::Apply(
            notice.nextRole,
            notice.nextContext,
            ResolveMemberClusterIdChange{
                VamClusterId{82},
                false,
                VamClusterId{83},
                VamClusterLeaveReason::SafetyCondition
            });

    Require(
        invalidLeaveId.transition == VruTransitionResult::Rejected,
        "leave indication must use either the old or new cluster ID");

    Require(
        invalidLeaveId.nextContext == notice.nextContext &&
            invalidLeaveId.nextRole.vbs == notice.nextRole.vbs,
        "invalid leave ID must preserve member state and context");

    Require(
        !invalidLeaveId.operation.has_value(),
        "invalid leave ID must not produce a VAM operation");

    // A member cannot resolve an ID change without first receiving notice.
    const auto prematureResolution =
        VruClusterLifecycleStateMachine::Apply(
            Passive(),
            memberContext,
            ResolveMemberClusterIdChange{
                VamClusterId{82},
                true,
                VamClusterId{81},
                VamClusterLeaveReason::NotProvided
            });

    Require(
        prematureResolution.transition == VruTransitionResult::Rejected,
        "member ID change resolution requires a pending notice");
}


void TestPersistedContextPayloadsAreValidated()
{
    auto requireRejectedAtomically =
        [](const VruRoleSnapshot& role,
           const VruClusterLifecycleContext& context,
           const VruClusterLifecycleEvent& event,
           const char* message)
        {
            const auto result =
                VruClusterLifecycleStateMachine::Apply(
                    role,
                    context,
                    event);

            Require(
                result.transition == VruTransitionResult::Rejected,
                message);

            Require(
                result.nextRole.role == role.role &&
                    result.nextRole.vbs == role.vbs,
                "invalid persisted context must preserve role");

            Require(
                result.nextContext == context,
                "invalid persisted context must be preserved atomically");

            Require(
                !result.operation.has_value(),
                "invalid persisted context must not produce an operation");

            Require(
                !result.error.empty(),
                "invalid persisted context must provide an error");
        };

    VruClusterLifecycleContext invalidJoining;
    invalidJoining.phase = VruClusterLifecyclePhase::Joining;
    invalidJoining.clusterId = VamClusterId{85};
    invalidJoining.joinTime = TimeMs(501);

    requireRejectedAtomically(
        ActiveStandalone(),
        invalidJoining,
        CompleteJoin{},
        "CompleteJoin must reject an invalid persisted join time");

    VruClusterLifecycleContext invalidBreakup;
    invalidBreakup.phase =
        VruClusterLifecyclePhase::LeaderBreakupWarning;
    invalidBreakup.clusterId = VamClusterId{86};
    invalidBreakup.breakupInfo = VamClusterBreakupInfo{
        VamClusterBreakupReason::ClusteringPurposeCompleted,
        TimeMs(501)
    };

    requireRejectedAtomically(
        ClusterLeader(),
        invalidBreakup,
        CompleteLeaderBreakup{},
        "leader breakup completion must reject invalid persisted time");

    VruClusterLifecycleContext invalidLeaderIdChange;
    invalidLeaderIdChange.phase = VruClusterLifecyclePhase::Leader;
    invalidLeaderIdChange.clusterId = VamClusterId{87};
    invalidLeaderIdChange.clusterIdChangeTime = TimeMs(501);

    requireRejectedAtomically(
        ClusterLeader(),
        invalidLeaderIdChange,
        CompleteLeaderClusterIdChange{VamClusterId{88}},
        "leader ID change completion must reject invalid persisted time");

    VruClusterLifecycleContext invalidMemberIdChange;
    invalidMemberIdChange.phase = VruClusterLifecyclePhase::Member;
    invalidMemberIdChange.clusterId = VamClusterId{89};
    invalidMemberIdChange.clusterIdChangeTime = TimeMs(501);

    requireRejectedAtomically(
        Passive(),
        invalidMemberIdChange,
        ResolveMemberClusterIdChange{
            VamClusterId{90},
            true,
            VamClusterId{89},
            VamClusterLeaveReason::NotProvided
        },
        "member ID change resolution must reject invalid persisted time");

    VruClusterLifecycleContext invalidLeave;
    invalidLeave.phase =
        VruClusterLifecyclePhase::LeavingNotification;
    invalidLeave.leaveInfo = VamClusterLeaveInfo{
        VamClusterId{91},
        static_cast<VamClusterLeaveReason>(255)
    };

    const VruRoleSnapshot activeStandalone = ActiveStandalone();

    requireRejectedAtomically(
        activeStandalone,
        invalidLeave,
        CompleteLeaveNotification{},
        "leave completion must reject an undefined persisted reason");
}

void TestInvalidTimeIsRejectedAtomically()
{
    const auto beforeRole = ActiveStandalone();
    const VruClusterLifecycleContext beforeContext;

    const auto invalid = VruClusterLifecycleStateMachine::Apply(
        beforeRole,
        beforeContext,
        BeginJoin{VamClusterId{61}, TimeMs(501)});

    Require(
        invalid.transition == VruTransitionResult::Rejected,
        "invalid join time must be rejected");

    Require(
        invalid.nextRole.role == beforeRole.role &&
            invalid.nextRole.vbs == beforeRole.vbs,
        "invalid event must not mutate role state");

    Require(
        invalid.nextContext == beforeContext,
        "invalid event must not mutate lifecycle context");

    Require(
        !invalid.operation.has_value(),
        "invalid event must not produce a VAM operation");

    Require(
        !invalid.error.empty(),
        "invalid event must explain the rejection");
}

} // namespace

int main()
{
    TestStandaloneAndClusterCreation();
    TestJoinLifecycle();
    TestJoinCancellationAndFailure();
    TestJoinEventsArePhaseSpecific();
    TestMemberLeaveAndLeaderLoss();
    TestLeaderBreakupLifecycle();
    TestMemberBreakupAndCpmException();
    TestLowRiskAndClusterIdChange();
    TestClusterIdChangeMemberLifecycle();
    TestPersistedContextPayloadsAreValidated();
    TestInvalidTimeIsRejectedAtomically();

    std::cout << "VAM_CLUSTER_LIFECYCLE_STATE_MACHINE=PASS\n";
    return EXIT_SUCCESS;
}
