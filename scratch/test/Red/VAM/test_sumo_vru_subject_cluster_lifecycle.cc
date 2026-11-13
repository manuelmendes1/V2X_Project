#include <cstdlib>
#include <iostream>
#include <optional>

#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/vam/cluster/vam_cluster_lifecycle_state_machine.h"

using ns3::SumoVruSubjectRegistry;

using namespace vanetza_integration::vam;

using vanetza_integration::vru::role::VbsState;
using vanetza_integration::vru::role::VruProfile;
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

void RequireRole(
    const SumoVruSubjectRegistry& registry,
    const std::string& personId,
    VruRoleStatus role,
    VbsState vbs,
    VruClusterLifecyclePhase phase,
    const char* message)
{
    const auto subject = registry.Find(personId);
    Require(subject.has_value(), message);
    Require(subject->roleState.role == role, message);
    Require(subject->roleState.vbs == vbs, message);
    Require(subject->clusterContext.phase == phase, message);
}

VamClusterOperationTime TimeMs(std::int64_t milliseconds)
{
    return {std::chrono::milliseconds{milliseconds}};
}

void PrepareStandalone(
    SumoVruSubjectRegistry& registry,
    const std::string& personId)
{
    Require(
        registry.Upsert(personId, VruProfile::Pedestrian),
        "subject creation must succeed");

    Require(
        registry.Activate(personId) == VruTransitionResult::Applied,
        "subject activation must succeed");

    Require(
        registry.SetVbsState(personId, VbsState::ActiveStandalone) ==
            VruTransitionResult::Applied,
        "explicit standalone activation must succeed");
}

void TestManualVbsTransitionsCannotBypassLifecycle()
{
    SumoVruSubjectRegistry registry;
    PrepareStandalone(registry, "manual-state");

    const auto passive =
        registry.SetVbsState("manual-state", VbsState::Passive);

    Require(
        passive == VruTransitionResult::Rejected,
        "manual transition to Passive must be rejected");

    const auto leader =
        registry.SetVbsState("manual-state", VbsState::ActiveClusterLeader);

    Require(
        leader == VruTransitionResult::Rejected,
        "manual transition to ClusterLeader must be rejected");

    const auto idle =
        registry.SetVbsState("manual-state", VbsState::Idle);

    Require(
        idle == VruTransitionResult::Rejected,
        "manual transition to Idle must be rejected");

    RequireRole(
        registry,
        "manual-state",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::None,
        "rejected manual transitions must preserve subject state");
}

void TestLeaderLifecycleIsPersistedPerSubject()
{
    SumoVruSubjectRegistry registry;
    PrepareStandalone(registry, "leader");

    const auto created = registry.ApplyClusterLifecycle(
        "leader",
        CreateCluster{VamClusterId{71}});

    Require(
        created.has_value() &&
            created->transition == VruTransitionResult::Applied,
        "cluster creation must be applied through the registry");

    Require(
        !created->operation.has_value(),
        "cluster creation must not fabricate cluster information");

    RequireRole(
        registry,
        "leader",
        VruRoleStatus::On,
        VbsState::ActiveClusterLeader,
        VruClusterLifecyclePhase::Leader,
        "registry must synchronize leader VBS state and context");

    auto leader = registry.Find("leader");
    Require(
        leader->clusterContext.clusterId == VamClusterId{71},
        "registry must retain the leader cluster ID");

    Require(
        registry.Upsert("leader", VruProfile::Unknown),
        "profile refresh must succeed");

    leader = registry.Find("leader");
    Require(
        leader->profile == VruProfile::Unknown,
        "profile refresh must update the profile");

    Require(
        leader->clusterContext.phase == VruClusterLifecyclePhase::Leader &&
            leader->clusterContext.clusterId == VamClusterId{71},
        "profile refresh must preserve cluster lifecycle context");

    const auto deactivation = registry.Deactivate("leader");

    Require(
        deactivation == VruTransitionResult::Rejected,
        "leader cannot be deactivated while it owns a cluster");

    RequireRole(
        registry,
        "leader",
        VruRoleStatus::On,
        VbsState::ActiveClusterLeader,
        VruClusterLifecyclePhase::Leader,
        "rejected leader deactivation must preserve state");

    const VamClusterBreakupInfo breakupInfo{
        VamClusterBreakupReason::ClusteringPurposeCompleted,
        TimeMs(3000)
    };

    const auto beginBreakup = registry.ApplyClusterLifecycle(
        "leader",
        BeginLeaderBreakup{breakupInfo, false});

    Require(
        beginBreakup.has_value() &&
            beginBreakup->transition == VruTransitionResult::Applied,
        "leader breakup must be applied");

    RequireRole(
        registry,
        "leader",
        VruRoleStatus::On,
        VbsState::ActiveClusterLeader,
        VruClusterLifecyclePhase::LeaderBreakupWarning,
        "leader warning phase must be persisted");

    const auto repeated = registry.ApplyClusterLifecycle(
        "leader",
        RepeatLeaderBreakupNotification{});

    Require(
        repeated.has_value() &&
            repeated->transition == VruTransitionResult::NoChange,
        "repeated breakup notification must be idempotent");

    Require(
        repeated->operation.has_value() &&
            repeated->operation->clusterBreakupInfo == breakupInfo,
        "repeated breakup notification must preserve operation content");

    const auto completed = registry.ApplyClusterLifecycle(
        "leader",
        CompleteLeaderBreakup{});

    Require(
        completed.has_value() &&
            completed->transition == VruTransitionResult::Applied,
        "leader breakup completion must be applied");

    RequireRole(
        registry,
        "leader",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::None,
        "completed leader breakup must synchronize state and clear context");

    Require(
        registry.Deactivate("leader") == VruTransitionResult::Applied,
        "leader may be deactivated after breakup is complete");
}

void TestJoinConfirmationAndLeaveLifecycle()
{
    SumoVruSubjectRegistry registry;
    PrepareStandalone(registry, "member");

    const auto begin = registry.ApplyClusterLifecycle(
        "member",
        BeginJoin{VamClusterId{0}, TimeMs(5000)});

    Require(
        begin.has_value() &&
            begin->transition == VruTransitionResult::Applied,
        "join initiation must be applied");

    Require(
        begin->operation.has_value() &&
            begin->operation->clusterJoinInfo.has_value(),
        "join initiation must expose clusterJoinInfo");

    RequireRole(
        registry,
        "member",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::Joining,
        "joining phase must be persisted without an early Passive transition");

    const auto completeJoin = registry.ApplyClusterLifecycle(
        "member",
        CompleteJoin{});

    Require(
        completeJoin.has_value() &&
            completeJoin->transition == VruTransitionResult::Applied,
        "join notification completion must be applied");

    RequireRole(
        registry,
        "member",
        VruRoleStatus::On,
        VbsState::Passive,
        VruClusterLifecyclePhase::MemberPendingConfirmation,
        "pending membership must synchronize Passive state");

    const auto confirmed = registry.ApplyClusterLifecycle(
        "member",
        ConfirmJoin{});

    Require(
        confirmed.has_value() &&
            confirmed->transition == VruTransitionResult::Applied,
        "membership confirmation must be applied");

    RequireRole(
        registry,
        "member",
        VruRoleStatus::On,
        VbsState::Passive,
        VruClusterLifecyclePhase::Member,
        "confirmed membership must retain Passive state");

    const auto leave = registry.ApplyClusterLifecycle(
        "member",
        LeaveCluster{
            VamClusterLeaveReason::OutOfClusterBoundingBox
        });

    Require(
        leave.has_value() &&
            leave->transition == VruTransitionResult::Applied,
        "member leave must be applied");

    Require(
        leave->operation.has_value() &&
            leave->operation->clusterLeaveInfo.has_value() &&
            leave->operation->clusterLeaveInfo->clusterId == VamClusterId{0},
        "member leave must preserve cluster ID zero");

    RequireRole(
        registry,
        "member",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::LeavingNotification,
        "leave notification phase must synchronize ActiveStandalone");

    Require(
        registry.Deactivate("member") == VruTransitionResult::Rejected,
        "subject cannot deactivate while leave notifications are pending");

    const auto repeated = registry.ApplyClusterLifecycle(
        "member",
        RepeatLeaveNotification{});

    Require(
        repeated.has_value() &&
            repeated->transition == VruTransitionResult::NoChange,
        "leave notification repetition must be idempotent");

    Require(
        repeated->operation.has_value() &&
            repeated->operation->clusterLeaveInfo.has_value() &&
            repeated->operation->clusterLeaveInfo->clusterLeaveReason ==
                VamClusterLeaveReason::OutOfClusterBoundingBox,
        "repeated leave must preserve its reason");

    const auto completedLeave = registry.ApplyClusterLifecycle(
        "member",
        CompleteLeaveNotification{});

    Require(
        completedLeave.has_value() &&
            completedLeave->transition == VruTransitionResult::Applied,
        "leave notification completion must be applied");

    RequireRole(
        registry,
        "member",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::None,
        "completed leave must clear lifecycle context");

    Require(
        registry.Deactivate("member") == VruTransitionResult::Applied,
        "subject may deactivate after leave notifications complete");
}


void TestClusterIdChangeContextIsPersisted()
{
    SumoVruSubjectRegistry registry;
    PrepareStandalone(registry, "id-leader");

    const auto created = registry.ApplyClusterLifecycle(
        "id-leader",
        CreateCluster{VamClusterId{71}});

    Require(
        created.has_value() &&
            created->transition == VruTransitionResult::Applied,
        "ID-change leader setup must succeed");

    const auto announce = registry.ApplyClusterLifecycle(
        "id-leader",
        AnnounceClusterIdChange{TimeMs(3000)});

    Require(
        announce.has_value() &&
            announce->transition == VruTransitionResult::NoChange,
        "leader ID-change announcement must not change VBS");

    Require(
        announce->operation.has_value() &&
            announce->operation->clusterIdChangeTimeInfo == TimeMs(3000),
        "leader announcement must expose clusterIdChangeTimeInfo");

    auto leader = registry.Find("id-leader");
    Require(
        leader.has_value() &&
            leader->clusterContext.clusterIdChangeTime.has_value() &&
            *leader->clusterContext.clusterIdChangeTime == TimeMs(3000),
        "registry must persist leader ID-change time");

    Require(
        leader->clusterContext.clusterId == VamClusterId{71},
        "announcement must not prematurely change leader cluster ID");

    Require(
        registry.Deactivate("id-leader") == VruTransitionResult::Rejected,
        "leader cannot deactivate while an ID change is pending");

    const auto changed = registry.ApplyClusterLifecycle(
        "id-leader",
        CompleteLeaderClusterIdChange{VamClusterId{72}});

    Require(
        changed.has_value() &&
            changed->transition == VruTransitionResult::Applied,
        "leader ID-change completion must be applied");

    leader = registry.Find("id-leader");
    Require(
        leader.has_value() &&
            leader->clusterContext.clusterId == VamClusterId{72},
        "registry must persist the new leader cluster ID");

    Require(
        !leader->clusterContext.clusterIdChangeTime.has_value(),
        "leader ID-change completion must clear pending time");

    // Confirmed member receives the same announcement and remains Passive.
    PrepareStandalone(registry, "id-member");

    const auto join = registry.ApplyClusterLifecycle(
        "id-member",
        BeginJoin{VamClusterId{81}, TimeMs(2500)});

    Require(
        join.has_value() &&
            join->transition == VruTransitionResult::Applied,
        "member ID-change setup must begin with join");

    Require(
        registry.ApplyClusterLifecycle(
            "id-member",
            CompleteJoin{})->transition == VruTransitionResult::Applied,
        "member join notification completion must succeed");

    Require(
        registry.ApplyClusterLifecycle(
            "id-member",
            ConfirmJoin{})->transition == VruTransitionResult::Applied,
        "member confirmation must succeed");

    const auto notice = registry.ApplyClusterLifecycle(
        "id-member",
        ReceiveClusterIdChangeNotice{TimeMs(3000)});

    Require(
        notice.has_value() &&
            notice->transition == VruTransitionResult::NoChange,
        "member ID-change notice must preserve VBS state");

    auto member = registry.Find("id-member");
    Require(
        member.has_value() &&
            member->roleState.vbs == VbsState::Passive,
        "member must remain Passive after receiving ID-change notice");

    Require(
        member->clusterContext.clusterId == VamClusterId{81} &&
            member->clusterContext.clusterIdChangeTime.has_value() &&
            *member->clusterContext.clusterIdChangeTime == TimeMs(3000),
        "registry must persist member old ID and pending change time");

    const auto matched = registry.ApplyClusterLifecycle(
        "id-member",
        ResolveMemberClusterIdChange{
            VamClusterId{82},
            true,
            VamClusterId{81},
            VamClusterLeaveReason::NotProvided
        });

    Require(
        matched.has_value() &&
            matched->transition == VruTransitionResult::Applied,
        "member matching the new cluster must update membership");

    member = registry.Find("id-member");
    Require(
        member.has_value() &&
            member->roleState.vbs == VbsState::Passive &&
            member->clusterContext.phase == VruClusterLifecyclePhase::Member &&
            member->clusterContext.clusterId == VamClusterId{82},
        "registry must persist matched new member ID without changing VBS");

    Require(
        !member->clusterContext.clusterIdChangeTime.has_value(),
        "successful member match must clear pending ID-change time");

    // A separate member cannot match the new cluster and therefore leaves.
    PrepareStandalone(registry, "id-member-leaving");

    Require(
        registry.ApplyClusterLifecycle(
            "id-member-leaving",
            BeginJoin{VamClusterId{91}, TimeMs(2500)})->transition ==
            VruTransitionResult::Applied,
        "second member join must begin");

    Require(
        registry.ApplyClusterLifecycle(
            "id-member-leaving",
            CompleteJoin{})->transition == VruTransitionResult::Applied,
        "second member join notification completion must succeed");

    Require(
        registry.ApplyClusterLifecycle(
            "id-member-leaving",
            ConfirmJoin{})->transition == VruTransitionResult::Applied,
        "second member confirmation must succeed");

    Require(
        registry.ApplyClusterLifecycle(
            "id-member-leaving",
            ReceiveClusterIdChangeNotice{TimeMs(3000)})->transition ==
            VruTransitionResult::NoChange,
        "second member ID-change notice must be recorded");

    const auto unmatched = registry.ApplyClusterLifecycle(
        "id-member-leaving",
        ResolveMemberClusterIdChange{
            VamClusterId{92},
            false,
            VamClusterId{92},
            VamClusterLeaveReason::SafetyCondition
        });

    Require(
        unmatched.has_value() &&
            unmatched->transition == VruTransitionResult::Applied,
        "unmatched member must begin the leave lifecycle");

    Require(
        unmatched->operation.has_value() &&
            unmatched->operation->clusterLeaveInfo.has_value() &&
            unmatched->operation->clusterLeaveInfo->clusterId ==
                VamClusterId{92},
        "registry must preserve caller-selected new ID for leave indication");

    RequireRole(
        registry,
        "id-member-leaving",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::LeavingNotification,
        "unmatched member must enter leave-notification phase");
}

void TestTerminalSumoRemovalDiscardsClusterContext()
{
    SumoVruSubjectRegistry registry;
    constexpr const char* personId = "removed-cluster-member";

    PrepareStandalone(registry, personId);

    const auto begin = registry.ApplyClusterLifecycle(
        personId,
        BeginJoin{VamClusterId{101}, TimeMs(2500)});

    Require(
        begin.has_value() &&
            begin->transition == VruTransitionResult::Applied,
        "terminal-removal setup must begin cluster join");

    const auto joinCompleted = registry.ApplyClusterLifecycle(
        personId,
        CompleteJoin{});

    Require(
        joinCompleted.has_value() &&
            joinCompleted->transition == VruTransitionResult::Applied,
        "terminal-removal setup must complete join notification");

    const auto joinConfirmed = registry.ApplyClusterLifecycle(
        personId,
        ConfirmJoin{});

    Require(
        joinConfirmed.has_value() &&
            joinConfirmed->transition == VruTransitionResult::Applied,
        "terminal-removal setup must confirm cluster membership");

    const auto beforeRemove = registry.Find(personId);
    Require(
        beforeRemove.has_value() &&
            beforeRemove->roleState.role == VruRoleStatus::On &&
            beforeRemove->roleState.vbs == VbsState::Passive &&
            beforeRemove->clusterContext.phase ==
                VruClusterLifecyclePhase::Member &&
            beforeRemove->clusterContext.clusterId == VamClusterId{101},
        "terminal-removal setup must persist confirmed membership");

    Require(
        registry.Remove(personId),
        "SUMO REMOVE must terminate an existing cluster member");

    Require(
        !registry.Find(personId).has_value(),
        "terminal SUMO REMOVE must erase the subject and cluster context");

    Require(
        !registry.Remove(personId),
        "repeated SUMO REMOVE must report no existing subject");

    Require(
        registry.Upsert(personId, VruProfile::Pedestrian),
        "subject re-entry after terminal removal must be accepted");

    const auto reentered = registry.Find(personId);
    Require(
        reentered.has_value(),
        "re-entered subject must exist in the registry");

    Require(
        reentered->roleState.role == VruRoleStatus::Off &&
            reentered->roleState.vbs == VbsState::Idle,
        "re-entered subject must start with a fresh role and VBS state");

    Require(
        reentered->clusterContext.phase == VruClusterLifecyclePhase::None &&
            !reentered->clusterContext.clusterId.has_value() &&
            !reentered->clusterContext.joinTime.has_value() &&
            !reentered->clusterContext.breakupInfo.has_value() &&
            !reentered->clusterContext.leaveInfo.has_value() &&
            !reentered->clusterContext.clusterIdChangeTime.has_value(),
        "re-entered subject must not inherit any prior cluster context");
}

void TestFailedJoinAndAtomicRejection()
{
    SumoVruSubjectRegistry registry;
    PrepareStandalone(registry, "failed-member");

    const auto begin = registry.ApplyClusterLifecycle(
        "failed-member",
        BeginJoin{VamClusterId{22}, TimeMs(3000)});

    Require(
        begin.has_value() &&
            begin->transition == VruTransitionResult::Applied,
        "failed-join setup must succeed");

    const auto prematureFailure = registry.ApplyClusterLifecycle(
        "failed-member",
        FailJoin{});

    Require(
        prematureFailure.has_value() &&
            prematureFailure->transition == VruTransitionResult::Rejected,
        "FailJoin during Joining must be rejected");

    RequireRole(
        registry,
        "failed-member",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::Joining,
        "rejected FailJoin must preserve registry state");

    const auto completeJoin = registry.ApplyClusterLifecycle(
        "failed-member",
        CompleteJoin{});

    Require(
        completeJoin.has_value() &&
            completeJoin->transition == VruTransitionResult::Applied,
        "join notification completion must succeed");

    const auto failed = registry.ApplyClusterLifecycle(
        "failed-member",
        FailJoin{});

    Require(
        failed.has_value() &&
            failed->transition == VruTransitionResult::Applied,
        "failed confirmation must be applied");

    RequireRole(
        registry,
        "failed-member",
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        VruClusterLifecyclePhase::LeavingNotification,
        "failed join must synchronize standalone leave-notification state");

    Require(
        failed->operation.has_value() &&
            failed->operation->clusterLeaveInfo.has_value() &&
            failed->operation->clusterLeaveInfo->clusterLeaveReason ==
                VamClusterLeaveReason::FailedJoin,
        "failed join must expose FailedJoin operation");

    const auto completed = registry.ApplyClusterLifecycle(
        "failed-member",
        CompleteLeaveNotification{});

    Require(
        completed.has_value() &&
            completed->transition == VruTransitionResult::Applied,
        "failed join leave notifications must be completable");

    Require(
        !registry.ApplyClusterLifecycle(
            "missing-member",
            EnterActiveStandalone{}).has_value(),
        "missing subject must return nullopt");
}

} // namespace

int main()
{
    TestManualVbsTransitionsCannotBypassLifecycle();
    TestLeaderLifecycleIsPersistedPerSubject();
    TestJoinConfirmationAndLeaveLifecycle();
    TestClusterIdChangeContextIsPersisted();
    TestFailedJoinAndAtomicRejection();
    TestTerminalSumoRemovalDiscardsClusterContext();

    std::cout << "SUMO_VRU_SUBJECT_CLUSTER_LIFECYCLE=PASS\n";
    return EXIT_SUCCESS;
}
