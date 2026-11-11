#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/context/sumo_person_vru_subject_synchronizer/sumo_person_vru_subject_synchronizer.h"
#include "vanetza_integration/vru/role/policy/vru_role_policy.h"

using ns3::SumoPersonDatagram;
using ns3::SumoPersonEventType;
using ns3::SumoPersonRegistry;
using ns3::SumoPersonSample;
using ns3::SumoVruSubjectRegistry;
using ns3::SumoVruSubjectSynchronizer;

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

SumoPersonDatagram MakeUpsert(
    const std::string& personId,
    std::uint64_t timestampUs,
    double x,
    std::optional<std::string> vClass)
{
    SumoPersonDatagram datagram;
    datagram.eventType = SumoPersonEventType::Upsert;
    datagram.personId = personId;
    datagram.sample = SumoPersonSample{x, 12.0, 1.5, 90.0};
    datagram.simulationTimeUs = timestampUs;
    datagram.sumoVClass = std::move(vClass);
    return datagram;
}

SumoPersonDatagram MakeRemove(
    const std::string& personId,
    std::uint64_t timestampUs)
{
    SumoPersonDatagram datagram;
    datagram.eventType = SumoPersonEventType::Remove;
    datagram.personId = personId;
    datagram.simulationTimeUs = timestampUs;
    return datagram;
}

void RequireRole(
    const ns3::SumoVruSubjectSnapshot& subject,
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
    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;

    Require(persons.Empty(), "Person registry must start empty");
    Require(subjects.Empty(), "Subject registry must start empty");

    // Empty ID must be rejected without mutating either registry.
    Require(
        !SumoVruSubjectSynchronizer::Apply(
            MakeUpsert("", 100, 1.0, std::string("pedestrian")),
            persons,
            subjects),
        "empty Person ID must be rejected");

    Require(persons.Empty(), "invalid event must not mutate Person registry");
    Require(subjects.Empty(), "invalid event must not create a subject");

    // Accepted Person UPSERT creates a corresponding OFF/IDLE subject.
    Require(
        SumoVruSubjectSynchronizer::Apply(
            MakeUpsert("person-a", 100, 10.0, std::string("pedestrian")),
            persons,
            subjects),
        "valid pedestrian UPSERT must be accepted");

    Require(persons.Size() == 1, "Person registry must contain person-a");
    Require(subjects.Size() == 1, "Subject registry must contain person-a");

    auto sourceA = persons.Find("person-a");
    auto subjectA = subjects.Find("person-a");

    Require(sourceA.has_value(), "Person source state must exist");
    Require(subjectA.has_value(), "VRU subject must exist");
    Require(sourceA->simulationTimeUs == 100, "source timestamp must be retained");
    Require(sourceA->sample.x == 10.0, "source mobility must be retained");
    Require(
        subjectA->profile == VruProfile::Pedestrian,
        "pedestrian vClass must map to Pedestrian");

    RequireRole(
        *subjectA,
        VruRoleStatus::Off,
        VbsState::Idle,
        "new subject must start OFF/IDLE");

    // Explicit management activates role and VBS independently.
    Require(
        subjects.Activate("person-a") == VruTransitionResult::Applied,
        "explicit activation must succeed");

    Require(
        subjects.SetVbsState("person-a", VbsState::ActiveStandalone) ==
            VruTransitionResult::Applied,
        "explicit standalone VBS transition must succeed");

    // Equal timestamp is rejected by the source registry. Neither registry
    // may be changed by the rejected profile/mobility update.
    Require(
        !SumoVruSubjectSynchronizer::Apply(
            MakeUpsert("person-a", 100, 99.0, std::string("passenger")),
            persons,
            subjects),
        "duplicate timestamp UPSERT must be rejected");

    sourceA = persons.Find("person-a");
    subjectA = subjects.Find("person-a");

    Require(sourceA->sample.x == 10.0, "rejected event must preserve source mobility");
    Require(
        subjectA->profile == VruProfile::Pedestrian,
        "rejected event must preserve source-derived profile");

    RequireRole(
        *subjectA,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "rejected event must preserve role and VBS state");

    // A newer event may update profile/mobility without resetting role state.
    Require(
        SumoVruSubjectSynchronizer::Apply(
            MakeUpsert("person-a", 200, 20.0, std::string("passenger")),
            persons,
            subjects),
        "newer Person UPSERT must be accepted");

    sourceA = persons.Find("person-a");
    subjectA = subjects.Find("person-a");

    Require(sourceA->simulationTimeUs == 200, "newer source timestamp must be stored");
    Require(sourceA->sample.x == 20.0, "newer source mobility must be stored");
    Require(
        subjectA->profile == VruProfile::Unknown,
        "unapproved vClass must map to Unknown");

    RequireRole(
        *subjectA,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "profile refresh must preserve role and VBS state");

    const auto permissions = VruRolePolicy::Evaluate(
        subjectA->roleState,
        subjectA->profile,
        false);

    Require(!permissions.transmit, "Unknown profile must block VAM TX");
    Require(permissions.receive, "ON role must retain current RX policy");
    Require(
        permissions.txReason == VruTxDecisionReason::UnsupportedProfile,
        "Unknown profile must report unsupported-profile TX reason");

    // Independent subjects must not share role state.
    Require(
        SumoVruSubjectSynchronizer::Apply(
            MakeUpsert("person-b", 100, 30.0, std::string("pedestrian")),
            persons,
            subjects),
        "second Person UPSERT must be accepted");

    Require(persons.Size() == 2, "two Person states must exist");
    Require(subjects.Size() == 2, "two VRU subjects must exist");

    const auto subjectB = subjects.Find("person-b");
    Require(subjectB.has_value(), "person-b subject must exist");

    RequireRole(
        *subjectB,
        VruRoleStatus::Off,
        VbsState::Idle,
        "person-b must have independent OFF/IDLE state");

    subjectA = subjects.Find("person-a");
    RequireRole(
        *subjectA,
        VruRoleStatus::On,
        VbsState::ActiveStandalone,
        "person-b creation must not affect person-a");

    // A stale REMOVE must not delete either source or derived state.
    Require(
        !SumoVruSubjectSynchronizer::Apply(
            MakeRemove("person-a", 150),
            persons,
            subjects),
        "stale REMOVE must be rejected");

    Require(
        persons.Find("person-a").has_value(),
        "stale REMOVE must preserve source Person");
    Require(
        subjects.Find("person-a").has_value(),
        "stale REMOVE must preserve VRU subject");

    // An accepted REMOVE removes both records.
    Require(
        SumoVruSubjectSynchronizer::Apply(
            MakeRemove("person-a", 300),
            persons,
            subjects),
        "newer REMOVE must be accepted");

    Require(
        !persons.Find("person-a").has_value(),
        "accepted REMOVE must delete source Person");
    Require(
        !subjects.Find("person-a").has_value(),
        "accepted REMOVE must delete VRU subject");

    // The source tombstone must prevent stale resurrection in both registries.
    Require(
        !SumoVruSubjectSynchronizer::Apply(
            MakeUpsert("person-a", 250, 50.0, std::string("pedestrian")),
            persons,
            subjects),
        "UPSERT older than REMOVE tombstone must be rejected");

    Require(
        !persons.Find("person-a").has_value(),
        "stale UPSERT must not recreate source Person");
    Require(
        !subjects.Find("person-a").has_value(),
        "stale UPSERT must not recreate VRU subject");

    // A newer re-entry creates a new source sample and a fresh OFF/IDLE subject.
    Require(
        SumoVruSubjectSynchronizer::Apply(
            MakeUpsert("person-a", 400, 60.0, std::string("pedestrian")),
            persons,
            subjects),
        "newer re-entry must be accepted");

    sourceA = persons.Find("person-a");
    subjectA = subjects.Find("person-a");

    Require(sourceA.has_value(), "re-entered source Person must exist");
    Require(subjectA.has_value(), "re-entered VRU subject must exist");
    Require(sourceA->simulationTimeUs == 400, "re-entry timestamp must be retained");
    Require(
        subjectA->profile == VruProfile::Pedestrian,
        "re-entry must use current vClass mapping");

    RequireRole(
        *subjectA,
        VruRoleStatus::Off,
        VbsState::Idle,
        "re-entry must not restore the previous role state");

    std::cout << "SUMO_PERSON_VRU_SUBJECT_SYNCHRONIZER=PASS\n";
    return EXIT_SUCCESS;
}
