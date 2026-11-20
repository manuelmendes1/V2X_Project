#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"

#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <cmath>
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
    bool activate,
    double y = 0.0)
{
    SumoPersonDatagram datagram{};
    datagram.eventType = SumoPersonEventType::Upsert;
    datagram.personId = personId;
    datagram.sample = SumoPersonSample{x, y, 1.0, 90.0};
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
            candidateB->decision.transactionId.has_value() &&
            candidateA->transactionHandle.has_value() &&
            candidateB->transactionHandle.has_value(),
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
            *candidateA->transactionHandle,
            0,
            error),
        "CONFIRMATION_RESOLVES_ONLY_THE_SELECTED_SUBJECT");

    error.clear();
    ok &= Check(
        scheduler.AbortGeneration(
            *candidateB->transactionHandle,
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


bool TestDrainPreservesIdentity()
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

    ok &= Check(
        scheduler.DrainTransactions(error) && error.empty(),
        "DRAIN_RESOLVES_PENDING_TRANSACTION");

    ok &= Check(
        scheduler.GetPendingCandidates().empty() &&
            authority.IsReservedBy(5001, owner),
        "DRAIN_CLEARS_RUNTIME_AND_PRESERVES_IDENTITY");

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
        "DRAIN_ALLOWS_FRESH_INITIAL_EVALUATION");

    return ok;
}


bool TestConfiguredProjectionBuildsWgs84PotiReference()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;

    auto config = Config();
    config.sumoProjection = SumoUtmWgs84ProjectionConfig{
        29,
        true,
        -526913.640,
        -4576484.190
    };

    VamGenerationScheduler scheduler(config);

    bool ok = AddPerson(
        persons,
        subjects,
        "ped-geodetic",
        0,
        20374.270,
        true,
        9640.360);

    ok &= Check(
        RegisterIdentity(scheduler, "ped-geodetic", 6101),
        "GEODETIC_FIXTURE_HAS_EXPLICIT_STATION_ID");

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler,
        100000,
        persons,
        subjects,
        context,
        candidates,
        report,
        error);

    const bool hasCandidate = candidates.size() == 1;
    ok &= Check(
        hasCandidate,
        "CONFIGURED_PROJECTION_GENERATES_CANDIDATE");

    if (hasCandidate)
    {
        const auto& input = candidates.front().input;

        const bool validPosition =
            input.referencePosition.latitudeDeg.has_value() &&
            input.referencePosition.longitudeDeg.has_value() &&
            std::abs(*input.referencePosition.latitudeDeg -
                     41.425290000) <= 1e-7 &&
            std::abs(*input.referencePosition.longitudeDeg -
                     (-8.434076200)) <= 1e-7;

        ok &= Check(
            validPosition,
            "CONFIGURED_PROJECTION_POPULATES_WGS84_REFERENCE_POSITION");

        ok &= Check(
            input.referencePositionTimeIts ==
                std::chrono::microseconds{kItsStartClockUs} &&
            input.generationTimeIts ==
                std::chrono::microseconds{kItsStartClockUs + 100000},
            "CONFIGURED_PROJECTION_USES_VALIDATED_POTI_TIME_REFERENCE");
    }

    std::string drainError;
    ok &= Check(
        scheduler.DrainTransactions(drainError) && drainError.empty(),
        "GEODETIC_FIXTURE_DRAINS_PENDING_TRANSACTION");

    return ok;
}


bool TestProjectionFailureIsAtomicAcrossSubjects()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;

    auto config = Config();
    config.sumoProjection = SumoUtmWgs84ProjectionConfig{
        29,
        true,
        -526913.640,
        -4576484.190
    };

    VamGenerationScheduler scheduler(config);

    bool ok = AddPerson(
        persons, subjects, "ped-a-valid", 0,
        20374.270, true, 9640.360);

    ok &= AddPerson(
        persons, subjects, "ped-b-invalid", 0,
        2000000.0, true, 9640.360);

    ok &= Check(
        RegisterIdentity(scheduler, "ped-a-valid", 6201) &&
        RegisterIdentity(scheduler, "ped-b-invalid", 6202),
        "ATOMIC_GEO_FIXTURE_IDENTITIES_REGISTERED");

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;

    VamGenerationScheduledCandidate sentinel{};
    sentinel.egoPersonId = "preexisting-output";
    candidates.push_back(sentinel);

    VamGenerationSchedulerReport report{};
    report.evaluatedSubjects = 777;

    std::string error;

    const bool accepted = RunStep(
        scheduler,
        100000,
        persons,
        subjects,
        context,
        candidates,
        report,
        error);

    ok &= Check(
        !accepted &&
        !error.empty() &&
        candidates.size() == 1 &&
        candidates.front().egoPersonId == "preexisting-output" &&
        report.evaluatedSubjects == 777,
        "PROJECTION_FAILURE_REJECTS_STEP_WITHOUT_PARTIAL_OUTPUT");

    ok &= Check(
        scheduler.GetPendingCandidates().empty(),
        "PROJECTION_FAILURE_DOES_NOT_COMMIT_STAGED_RUNTIME");

    // Repair the second sample at a later source timestamp, but retry the
    // exact same scheduler reference time. A failed step must not commit
    // cadence or logical-time state.
    SumoPersonDatagram repaired{};
    repaired.eventType = SumoPersonEventType::Upsert;
    repaired.personId = "ped-b-invalid";
    repaired.sample = SumoPersonSample{
        21374.270, 9640.360, 1.0, 90.0
    };
    repaired.simulationTimeUs = 1000;
    repaired.sumoVClass = "pedestrian";

    ok &= Check(
        persons.Apply(repaired),
        "INVALID_GEO_SAMPLE_CAN_BE_REPLACED");

    candidates.clear();
    report = {};
    error.clear();

    const bool retried = RunStep(
        scheduler,
        100000,
        persons,
        subjects,
        context,
        candidates,
        report,
        error);

    ok &= Check(
        retried &&
        error.empty() &&
        report.evaluatedSubjects == 2 &&
        candidates.size() == 2,
        "PROJECTION_FAILURE_CAN_RETRY_SAME_LOGICAL_STEP");

    std::string drainError;
    ok &= Check(
        scheduler.DrainTransactions(drainError) && drainError.empty(),
        "ATOMIC_GEO_RETRY_DRAINS_PENDING_TRANSACTIONS");

    return ok;
}


bool TestConfiguredProjectionRejectsPositionAgeBoundary()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;

    auto config = Config();
    config.sumoProjection = SumoUtmWgs84ProjectionConfig{
        29,
        true,
        -526913.640,
        -4576484.190
    };

    VamGenerationScheduler scheduler(config);

    bool ok = AddPerson(
        persons,
        subjects,
        "ped-stale-geodetic",
        0,
        20374.270,
        true,
        9640.360);

    ok &= Check(
        RegisterIdentity(scheduler, "ped-stale-geodetic", 6301),
        "STALE_GEO_FIXTURE_HAS_EXPLICIT_STATION_ID");

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    report.evaluatedSubjects = 888;
    std::string error;

    const bool accepted = RunStep(
        scheduler,
        32767000,
        persons,
        subjects,
        context,
        candidates,
        report,
        error);

    ok &= Check(
        !accepted &&
        error.find("position age must be strictly below 32767 ms") !=
            std::string::npos,
        "EXACT_32767_MS_POSITION_AGE_IS_REJECTED");

    ok &= Check(
        candidates.empty() &&
        report.evaluatedSubjects == 888 &&
        scheduler.GetPendingCandidates().empty(),
        "STALE_POSITION_REJECTION_PRESERVES_OUTPUT_AND_RUNTIME");

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
    ok &= TestDrainPreservesIdentity();
    ok &= TestConfiguredProjectionBuildsWgs84PotiReference();
    ok &= TestProjectionFailureIsAtomicAcrossSubjects();
    ok &= TestConfiguredProjectionRejectsPositionAgeBoundary();

    std::cout << "VAM_GENERATION_SCHEDULER="
              << (ok ? "PASS" : "FAIL") << '\n';

    return ok ? 0 : 1;
}
