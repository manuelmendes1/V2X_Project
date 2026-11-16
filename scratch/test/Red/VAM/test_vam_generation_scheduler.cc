#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"

#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;
using namespace vanetza_integration::vam;
using namespace vanetza_integration::vru::role;

namespace
{

constexpr std::int64_t kItsStartClockUs = 719748000000000LL;

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << label << '\n';
    return condition;
}

VamGenerationSchedulerConfig Config()
{
    VamGenerationSchedulerConfig config{};
    config.itsStartClockUs = kItsStartClockUs;
    config.generation.tCheckVamGenMs = 100;
    config.generation.tGenVamMinMs = 100;
    config.generation.tGenVamMaxMs = 5000;
    config.evaluation.snapshotBuilder.vehicleThresholds =
        {2.0, 2.0, 1.0};
    config.evaluation.snapshotBuilder.vruThresholds =
        {2.0, 2.0, 1.0};
    config.evaluation.proximity.maximumSampleAgeUs = 250000;
    return config;
}

bool AddPerson(
    SumoPersonRegistry& persons,
    SumoVruSubjectRegistry& subjects,
    const std::string& personId,
    std::uint64_t timeUs,
    double x,
    bool activate)
{
    SumoPersonDatagram datagram{};
    datagram.eventType = SumoPersonEventType::Upsert;
    datagram.personId = personId;
    datagram.sample = SumoPersonSample{x, 0.0, 1.0, 90.0};
    datagram.simulationTimeUs = timeUs;
    datagram.sumoVClass = "pedestrian";

    if (!persons.Apply(datagram) ||
        !subjects.Upsert(personId, VruProfile::Pedestrian))
    {
        return false;
    }

    if (!activate)
    {
        return true;
    }

    const auto role = subjects.Activate(personId);
    if (!role || *role != VruTransitionResult::Applied)
    {
        return false;
    }

    const auto vbs =
        subjects.SetVbsState(personId, VbsState::ActiveStandalone);

    return vbs && *vbs == VruTransitionResult::Applied;
}

bool RegisterIdentity(
    VamGenerationScheduler& scheduler,
    const std::string& personId,
    std::uint32_t stationId)
{
    std::string error;
    return scheduler.RegisterSubjectStationIdentity(
        personId, VamStationId{stationId}, error);
}

bool RunStep(
    VamGenerationScheduler& scheduler,
    std::uint64_t timeUs,
    const SumoPersonRegistry& persons,
    const SumoVruSubjectRegistry& subjects,
    const VamGenerationContext& context,
    std::vector<VamGenerationScheduledCandidate>& candidates,
    VamGenerationSchedulerReport& report,
    std::string& error)
{
    return scheduler.EvaluateStep(
        timeUs,
        persons,
        subjects,
        VehicleRegistry::Instance(),
        SumoVehicleLifecycleRegistry::Instance(),
        context,
        candidates,
        report,
        error);
}

bool TestExplicitIdentityAndInitialCandidate()
{
    auto& authority = ItsStationIdentityAuthority::Instance();
    authority.Clear();

    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-0", 0, 0.0, true);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    // A SUMO person must not be assigned an ITS identity implicitly.
    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        report.skippedMissingIdentity == 1 &&
            report.evaluatedSubjects == 0 &&
            candidates.empty(),
        "SUBJECT_REQUIRES_EXPLICIT_STATION_ID");

    ok &= Check(
        RegisterIdentity(scheduler, "ped-0", 1001),
        "EXPLICIT_STATION_ID_REGISTRATION_ACCEPTED");

    const ItsStationIdentityOwner owner{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:ped-0"
    };

    ok &= Check(
        authority.IsReservedBy(1001, owner),
        "VAM_IDENTITY_IS_RESERVED_BY_ITS_AUTHORITY");

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    const bool oneCandidate = candidates.size() == 1;
    ok &= Check(
        oneCandidate &&
            candidates.front().egoPersonId == "ped-0" &&
            candidates.front().stationId.value == 1001 &&
            candidates.front().decision.outcome ==
                VamGenerationOutcome::GenerationCandidate &&
            candidates.front().decision.transactionId.has_value(),
        "INITIAL_RUNTIME_EVALUATION_OPENS_CANDIDATE");

    if (oneCandidate)
    {
        const auto& input = candidates.front().input;

        ok &= Check(
            input.originatingStation.stationId.value == 1001 &&
                input.vruProfile == VruProfile::Pedestrian &&
                input.vruRole.role == VruRoleStatus::On &&
                input.vruRole.vbs == VbsState::ActiveStandalone,
            "CANDIDATE_PRESERVES_EXPLICIT_IDENTITY_AND_ROLE");

        ok &= Check(
            !input.referencePosition.latitudeDeg.has_value() &&
                !input.referencePosition.longitudeDeg.has_value() &&
                input.highFrequencyState.speedMps == 1.0 &&
                input.highFrequencyState.headingDeg == 90.0,
            "PROJECTED_XY_IS_NOT_MAPPED_TO_WGS84");

        ok &= Check(
            input.referencePositionTimeIts ==
                std::chrono::microseconds{kItsStartClockUs} &&
                input.generationTimeIts ==
                std::chrono::microseconds{kItsStartClockUs + 100000},
            "INPUT_TIMESTAMPS_USE_ITS_EPOCH_PLUS_SUMO_TIME");
    }

    ok &= Check(
        scheduler.GetPendingCandidates().size() == 1,
        "CANDIDATE_REMAINS_PENDING_UNTIL_RESOLVED");

    return ok;
}

bool TestPerSubjectTransactionIsolation()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(persons, subjects, "ped-a", 0, 0.0, true);
    ok &= AddPerson(persons, subjects, "ped-b", 0, 10.0, true);
    ok &= RegisterIdentity(scheduler, "ped-a", 2001);
    ok &= RegisterIdentity(scheduler, "ped-b", 2002);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        candidates.size() == 2,
        "EACH_ACTIVE_SUBJECT_HAS_AN_INDEPENDENT_MANAGER");

    const auto findCandidate =
        [&candidates](const std::string& personId)
            -> const VamGenerationScheduledCandidate*
        {
            for (const auto& candidate : candidates)
            {
                if (candidate.egoPersonId == personId)
                {
                    return &candidate;
                }
            }
            return nullptr;
        };

    const auto* candidateA = findCandidate("ped-a");
    const auto* candidateB = findCandidate("ped-b");

    ok &= Check(
        candidateA && candidateB &&
            candidateA->decision.transactionId.has_value() &&
            candidateB->decision.transactionId.has_value(),
        "SUBJECTS_HAVE_DISTINCT_PENDING_TRANSACTIONS");

    if (!candidateA || !candidateB ||
        !candidateA->decision.transactionId ||
        !candidateB->decision.transactionId)
    {
        return false;
    }

    error.clear();
    ok &= Check(
        scheduler.ConfirmGeneration(
            "ped-a",
            *candidateA->decision.transactionId,
            0,
            error),
        "CONFIRMATION_RESOLVES_ONLY_THE_SELECTED_SUBJECT");

    error.clear();
    ok &= Check(
        scheduler.AbortGeneration(
            "ped-b",
            *candidateB->decision.transactionId,
            error),
        "ABORT_RESOLVES_ONLY_THE_SELECTED_SUBJECT");

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        candidates.size() == 1 &&
            candidates.front().egoPersonId == "ped-b" &&
            candidates.front().decision.outcome ==
                VamGenerationOutcome::GenerationCandidate,
        "ABORTED_SUBJECT_CAN_RETRY_WITHOUT_RESETTING_CONFIRMED_PEER");

    ok &= Check(
        scheduler.GetPendingCandidates().size() == 1 &&
            scheduler.GetPendingCandidates().front().egoPersonId == "ped-b",
        "PENDING_CANDIDATE_STATE_IS_ISOLATED_PER_SUBJECT");

    return ok;
}

bool TestCadenceAndTimeRegression()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-off", 0, 0.0, false);
    ok &= RegisterIdentity(scheduler, "ped-off", 3001);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        report.blockedByPolicy == 1 && candidates.empty(),
        "ROLE_OFF_IS_REPORTED_AS_POLICY_BLOCK");

    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 50000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        report.skippedByCadence &&
            report.evaluatedSubjects == 0,
        "CHECK_INTERVAL_USES_LOGICAL_SUMO_TIME");

    // An out-of-order step must fail without modifying caller output.
    report.evaluatedSubjects = 77;
    candidates.emplace_back();
    error.clear();

    const bool regressed = RunStep(
        scheduler, 40000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        !regressed &&
            error.find("regress") != std::string::npos &&
            report.evaluatedSubjects == 77 &&
            candidates.size() == 1,
        "TIME_REGRESSION_IS_REJECTED_WITHOUT_OUTPUT_MUTATION");

    const auto activation = subjects.Activate("ped-off");
    const auto vbs =
        subjects.SetVbsState("ped-off", VbsState::ActiveStandalone);

    ok &= Check(
        activation && *activation == VruTransitionResult::Applied &&
            vbs && *vbs == VruTransitionResult::Applied,
        "EXPLICIT_ROLE_AND_VBS_ACTIVATION_ACCEPTED");

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        candidates.size() == 1 &&
            candidates.front().egoPersonId == "ped-off",
        "ELIGIBILITY_CHANGE_IS_OBSERVED_ON_NEXT_CHECK");

    return ok;
}

bool TestRemovalAndReentry()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-reentry", 0, 0.0, true);
    ok &= RegisterIdentity(scheduler, "ped-reentry", 4001);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        scheduler.GetPendingCandidates().size() == 1,
        "INITIAL_CANDIDATE_EXISTS_BEFORE_REMOVE");

    SumoPersonDatagram removal{};
    removal.eventType = SumoPersonEventType::Remove;
    removal.personId = "ped-reentry";
    removal.simulationTimeUs = 50000;

    ok &= Check(
        persons.Apply(removal) && subjects.Remove("ped-reentry"),
        "SUMO_REMOVE_TERMINATES_PERSON_AND_SUBJECT");

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        scheduler.GetPendingCandidates().empty(),
        "REMOVE_FORGETS_RUNTIME_MANAGER_AND_PENDING_CANDIDATE");

    const ItsStationIdentityOwner owner{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:ped-reentry"
    };

    ok &= Check(
        ItsStationIdentityAuthority::Instance().IsReservedBy(4001, owner),
        "SUMO_REMOVE_DOES_NOT_SILENTLY_RELEASE_LOGICAL_ITS_IDENTITY");

    ok &= AddPerson(
        persons, subjects, "ped-reentry", 200000, 5.0, true);

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 200000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        candidates.size() == 1 &&
            candidates.front().stationId.value == 4001 &&
            candidates.front().decision.reason ==
                VamGenerationReason::InitialActivation,
        "REENTRY_STARTS_NEW_RUNTIME_GENERATION_HISTORY");

    return ok;
}


bool TestRuntimeResetPreservesIdentity()
{
    auto& authority = ItsStationIdentityAuthority::Instance();
    authority.Clear();

    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-reset", 0, 0.0, true);
    ok &= RegisterIdentity(scheduler, "ped-reset", 5001);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        candidates.size() == 1 &&
            scheduler.GetPendingCandidates().size() == 1,
        "RESET_FIXTURE_HAS_PENDING_CANDIDATE");

    const ItsStationIdentityOwner owner{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:ped-reset"
    };

    scheduler.ResetRuntime();

    ok &= Check(
        scheduler.GetPendingCandidates().empty() &&
            authority.IsReservedBy(5001, owner),
        "RESET_CLEARS_PENDING_RUNTIME_ONLY");

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 50000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        candidates.size() == 1 &&
            candidates.front().stationId.value == 5001 &&
            candidates.front().decision.reason ==
                VamGenerationReason::InitialActivation &&
            !report.skippedByCadence,
        "RESET_ALLOWS_FRESH_INITIAL_EVALUATION");

    return ok;
}

} // namespace

int main()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    bool ok = true;

    ok &= TestExplicitIdentityAndInitialCandidate();
    ok &= TestPerSubjectTransactionIsolation();
    ok &= TestCadenceAndTimeRegression();
    ok &= TestRemovalAndReentry();
    ok &= TestRuntimeResetPreservesIdentity();

    std::cout << "VAM_GENERATION_SCHEDULER="
              << (ok ? "PASS" : "FAIL") << '\n';

    return ok ? 0 : 1;
}
