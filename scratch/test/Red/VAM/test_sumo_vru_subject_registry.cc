#include <cstdlib>
#include <iostream>

#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/vru/role/policy/vru_role_policy.h"

using ns3::SumoVruSubjectRegistry;
using ns3::SumoVruSubjectSnapshot;

using vanetza_integration::vru::role::VbsState;
using vanetza_integration::vru::role::VruProfile;
using vanetza_integration::vru::role::VruRolePolicy;
using vanetza_integration::vru::role::VruRoleStatus;
using vanetza_integration::vru::role::VruTransitionResult;
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

void RequireRole(
    const SumoVruSubjectSnapshot& subject,
    VruRoleStatus role,
    VbsState vbs,
    const char* message)
{
    Require(subject.roleState.role == role, message);
    Require(subject.roleState.vbs == vbs, message);
}

} // namespace

int
main()
{
    SumoVruSubjectRegistry registry;

    Require(registry.Empty(), "registry must initially be empty");
    Require(registry.Size() == 0, "initial size must be zero");

    Require(
        !registry.Upsert("", VruProfile::Pedestrian),
        "empty person ID must be rejected");

    Require(
        !registry.Find(""),
        "empty person ID must not resolve");

    // A new SUMO Person creates an independent OFF/IDLE subject.
    Require(
        registry.Upsert("person-a", VruProfile::Pedestrian),
        "first subject must be created");

    Require(registry.Size() == 1, "one subject must be stored");

    auto personA = registry.Find("person-a");
    Require(personA.has_value(), "created subject must be findable");
    Require(
        personA->profile == VruProfile::Pedestrian,
        "new subject must retain mapped profile");

    RequireRole(
        *personA,
        VruRoleStatus::Off,
        VbsState::Idle,
        "new subject must start OFF/IDLE");

    // Role activation is explicit and does not activate VBS implicitly.
    auto activation = registry.Activate("person-a");
    Require(
        activation.has_value() &&
            *activation == VruTransitionResult::Applied,
        "explicit activation must be applied");

    personA = registry.Find("person-a");
    RequireRole(
        *personA,
        VruRoleStatus::On,
        VbsState::Idle,
        "activation must result in ON/IDLE");

    activation = registry.Activate("person-a");
    Require(
        activation.has_value() &&
            *activation == VruTransitionResult::NoChange,
        "repeated activation must be idempotent");

    // VBS state is a separate explicit transition.
    auto vbsResult =
        registry.SetVbsState("person-a", VbsState::ActiveStandalone);

    Require(
        vbsResult.has_value() &&
            *vbsResult == VruTransitionResult::Applied,
        "standalone VBS transition must be applied");

    personA = registry.Find("person-a");
    RequireRole(
        *personA,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "subject must enter ON/ACTIVE_STANDALONE");

    // An accepted profile refresh must not reset the role or VBS state.
    Require(
        registry.Upsert("person-a", VruProfile::Unknown),
        "existing subject profile must be refreshable");

    personA = registry.Find("person-a");
    Require(personA.has_value(), "subject must survive profile refresh");
    Require(
        personA->profile == VruProfile::Unknown,
        "profile refresh must replace the old profile");

    RequireRole(
        *personA,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "profile refresh must preserve role and VBS state");

    // Unknown profile must remain unable to transmit, even when active.
    const auto permissions = VruRolePolicy::Evaluate(
        personA->roleState,
        personA->profile,
        false);

    Require(
        !permissions.transmit,
        "unknown profile must never authorize VAM TX");

    Require(
        permissions.receive,
        "active role must retain the existing RX policy");

    Require(
        permissions.txReason ==
            VruTxDecisionReason::UnsupportedProfile,
        "unknown profile must report unsupported-profile TX reason");

    // A second Person must have independent role state.
    Require(
        registry.Upsert("person-b", VruProfile::Pedestrian),
        "second subject must be created");

    Require(registry.Size() == 2, "two subjects must be stored");

    const auto personB = registry.Find("person-b");
    Require(personB.has_value(), "second subject must be findable");

    RequireRole(
        *personB,
        VruRoleStatus::Off,
        VbsState::Idle,
        "second subject must start OFF/IDLE");

    personA = registry.Find("person-a");
    RequireRole(
        *personA,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "creating another subject must not alter the first");

    // OFF must reject a non-IDLE VBS transition.
    Require(
        registry.Upsert("person-c", VruProfile::Unknown),
        "third subject must be created");

    const auto rejectedVbs =
        registry.SetVbsState("person-c", VbsState::ActiveStandalone);

    Require(
        rejectedVbs.has_value() &&
            *rejectedVbs == VruTransitionResult::Rejected,
        "OFF subject must reject non-IDLE VBS state");

    const auto personC = registry.Find("person-c");
    Require(personC.has_value(), "third subject must remain present");

    RequireRole(
        *personC,
        VruRoleStatus::Off,
        VbsState::Idle,
        "rejected VBS transition must preserve OFF/IDLE");

    // Missing subjects must be distinct from rejected state transitions.
    Require(
        !registry.Activate("missing-person").has_value(),
        "activation of missing subject must return nullopt");

    Require(
        !registry.Deactivate("missing-person").has_value(),
        "deactivation of missing subject must return nullopt");

    Require(
        !registry.SetVbsState(
            "missing-person", VbsState::ActiveStandalone).has_value(),
        "VBS update of missing subject must return nullopt");

    // Deactivation must reset both role and VBS state.
    const auto deactivation = registry.Deactivate("person-a");
    Require(
        deactivation.has_value() &&
            *deactivation == VruTransitionResult::Applied,
        "deactivation must be applied");

    personA = registry.Find("person-a");
    RequireRole(
        *personA,
        VruRoleStatus::Off,
        VbsState::Idle,
        "deactivation must reset subject to OFF/IDLE");

    Require(
        registry.Deactivate("person-a") == VruTransitionResult::NoChange,
        "repeated deactivation must be idempotent");

    // REMOVE must eliminate the entire subject and its role state.
    Require(
        registry.Remove("person-a"),
        "existing subject must be removed");

    Require(
        !registry.Find("person-a").has_value(),
        "removed subject must no longer be findable");

    Require(
        !registry.Remove("person-a"),
        "removing an absent subject must report false");

    // Re-entry creates a fresh subject, never restoring the old role.
    Require(
        registry.Upsert("person-a", VruProfile::Pedestrian),
        "re-entry must create a new subject");

    personA = registry.Find("person-a");
    Require(personA.has_value(), "re-entered subject must be findable");

    RequireRole(
        *personA,
        VruRoleStatus::Off,
        VbsState::Idle,
        "re-entry must create a fresh OFF/IDLE role state");

    Require(
        personA->profile == VruProfile::Pedestrian,
        "re-entry must use the current mapped profile");

    Require(registry.Size() == 3, "registry must contain three active subjects");

    registry.Clear();

    Require(registry.Empty(), "Clear must empty the registry");
    Require(registry.Size() == 0, "Clear must reset the size");
    Require(
        !registry.Find("person-b").has_value(),
        "Clear must remove every subject");

    std::cout << "SUMO_VRU_SUBJECT_REGISTRY=PASS\n";
    return EXIT_SUCCESS;
}
