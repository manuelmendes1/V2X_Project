#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"

#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
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


bool TestStalePresenceTransactionCannotConfirmReentry()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-stale-presence", 0, 0.0, true);
    ok &= RegisterIdentity(scheduler, "ped-stale-presence", 6101);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    const auto oldPending = scheduler.GetPendingCandidates();

    if (oldPending.size() != 1 ||
        !oldPending.front().decision.transactionId.has_value())
    {
        return Check(
            false,
            "STALE_PRESENCE_FIXTURE_HAS_INITIAL_TRANSACTION");
    }

    const auto oldTransactionId =
        *oldPending.front().decision.transactionId;

    if (!oldPending.front().transactionHandle.has_value())
    {
        return Check(false,
            "OLD_PRESENCE_FIXTURE_HAS_OPAQUE_TRANSACTION_HANDLE");
    }
    const auto oldHandle = *oldPending.front().transactionHandle;

    ok &= Check(
        oldTransactionId == 1,
        "OLD_PRESENCE_USES_LOCAL_TRANSACTION_ONE");

    SumoPersonDatagram removal{};
    removal.eventType = SumoPersonEventType::Remove;
    removal.personId = "ped-stale-presence";
    removal.simulationTimeUs = 50000;

    ok &= Check(
        persons.Apply(removal) &&
            subjects.Remove("ped-stale-presence"),
        "STALE_PRESENCE_FIXTURE_ACCEPTS_REMOVE");

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        scheduler.GetPendingCandidates().empty(),
        "OLD_PRESENCE_RUNTIME_IS_PRUNED");

    ok &= AddPerson(
        persons, subjects, "ped-stale-presence",
        200000, 5.0, true);

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 200000, persons, subjects, context,
        candidates, report, error);

    const auto newPending = scheduler.GetPendingCandidates();

    if (newPending.size() != 1 ||
        !newPending.front().decision.transactionId.has_value())
    {
        return Check(
            false,
            "NEW_PRESENCE_FIXTURE_HAS_TRANSACTION");
    }

    const auto newTransactionId =
        *newPending.front().decision.transactionId;

    if (!newPending.front().transactionHandle.has_value())
    {
        return Check(false,
            "NEW_PRESENCE_FIXTURE_HAS_OPAQUE_TRANSACTION_HANDLE");
    }
    const auto newHandle = *newPending.front().transactionHandle;

    ok &= Check(
        newTransactionId == oldTransactionId,
        "REENTRY_REPRODUCES_LOCAL_TRANSACTION_ID_COLLISION");
    ok &= Check(
        oldHandle != newHandle,
        "REENTRY_ISSUES_DISTINCT_PRESENCE_HANDLE");
    ok &= Check(
        oldHandle.SchedulerInstanceId() == newHandle.SchedulerInstanceId() &&
            oldHandle.PersonId() == newHandle.PersonId() &&
            oldHandle.PresenceEpoch() != newHandle.PresenceEpoch() &&
            oldHandle.TransactionId() == newHandle.TransactionId(),
        "REENTRY_HANDLE_DIFFERS_BY_PRESENCE_ONLY");

    error.clear();

    const bool staleConfirmationAccepted =
        scheduler.ConfirmGeneration(oldHandle, 200, error);

    ok &= Check(
        !staleConfirmationAccepted,
        "STALE_PRESENCE_TRANSACTION_CANNOT_CONFIRM_REENTRY");

    error.clear();
    const bool staleAbortAccepted =
        scheduler.AbortGeneration(oldHandle, error);
    ok &= Check(
        !staleAbortAccepted,
        "STALE_PRESENCE_TRANSACTION_CANNOT_ABORT_REENTRY");

    const auto pendingAfterAttempt =
        scheduler.GetPendingCandidates();

    ok &= Check(
        pendingAfterAttempt.size() == 1 &&
            pendingAfterAttempt.front().decision.transactionId ==
                newPending.front().decision.transactionId,
        "STALE_CONFIRMATION_DOES_NOT_MUTATE_NEW_PENDING_TRANSACTION");

    error.clear();
    ok &= Check(
        scheduler.ConfirmGeneration(newHandle, 200, error),
        "CURRENT_PRESENCE_HANDLE_CONFIRMS_CURRENT_TRANSACTION");

    error.clear();
    ok &= Check(
        !scheduler.ConfirmGeneration(newHandle, 200, error),
        "CONFIRMED_TRANSACTION_HANDLE_CANNOT_BE_REUSED");

    return ok;
}

bool TestOldSchedulerTransactionCannotAbortNewScheduler()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;

    VamGenerationScheduler oldScheduler(Config());
    VamGenerationScheduler newScheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-stale-scheduler", 0, 0.0, true);
    ok &= RegisterIdentity(
        oldScheduler, "ped-stale-scheduler", 6201);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        oldScheduler, 0, persons, subjects, context,
        candidates, report, error);

    const auto oldPending = oldScheduler.GetPendingCandidates();

    if (oldPending.size() != 1 ||
        !oldPending.front().decision.transactionId.has_value())
    {
        return Check(
            false,
            "OLD_SCHEDULER_FIXTURE_HAS_TRANSACTION");
    }

    const auto oldTransactionId =
        *oldPending.front().decision.transactionId;
    if (!oldPending.front().transactionHandle.has_value())
    {
        return Check(false,
            "OLD_SCHEDULER_FIXTURE_HAS_OPAQUE_TRANSACTION_HANDLE");
    }
    const auto oldHandle = *oldPending.front().transactionHandle;

    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        newScheduler, 0, persons, subjects, context,
        candidates, report, error);

    const auto newPending = newScheduler.GetPendingCandidates();

    if (newPending.size() != 1 ||
        !newPending.front().decision.transactionId.has_value())
    {
        return Check(
            false,
            "NEW_SCHEDULER_FIXTURE_HAS_TRANSACTION");
    }

    const auto newTransactionId =
        *newPending.front().decision.transactionId;
    if (!newPending.front().transactionHandle.has_value())
    {
        return Check(false,
            "NEW_SCHEDULER_FIXTURE_HAS_OPAQUE_TRANSACTION_HANDLE");
    }
    const auto newHandle = *newPending.front().transactionHandle;

    ok &= Check(
        oldTransactionId == 1 && newTransactionId == 1,
        "SCHEDULERS_REPRODUCE_LOCAL_TRANSACTION_ID_COLLISION");
    ok &= Check(
        oldHandle != newHandle,
        "SCHEDULERS_ISSUE_DISTINCT_INSTANCE_HANDLES");
    ok &= Check(
        oldHandle.SchedulerInstanceId() != newHandle.SchedulerInstanceId() &&
            oldHandle.PersonId() == newHandle.PersonId() &&
            oldHandle.PresenceEpoch() == newHandle.PresenceEpoch() &&
            oldHandle.TransactionId() == newHandle.TransactionId(),
        "SCHEDULER_HANDLE_DIFFERS_BY_INSTANCE_ONLY");

    error.clear();

    const bool staleAbortAccepted =
        newScheduler.AbortGeneration(oldHandle, error);

    ok &= Check(
        !staleAbortAccepted,
        "OLD_SCHEDULER_TRANSACTION_CANNOT_ABORT_NEW_SCHEDULER");

    error.clear();
    const bool staleConfirmationAccepted =
        newScheduler.ConfirmGeneration(oldHandle, 0, error);
    ok &= Check(
        !staleConfirmationAccepted,
        "OLD_SCHEDULER_TRANSACTION_CANNOT_CONFIRM_NEW_SCHEDULER");

    const auto pendingAfterAttempt =
        newScheduler.GetPendingCandidates();

    ok &= Check(
        pendingAfterAttempt.size() == 1 &&
            pendingAfterAttempt.front().decision.transactionId ==
                newPending.front().decision.transactionId,
        "STALE_ABORT_DOES_NOT_MUTATE_NEW_SCHEDULER_TRANSACTION");

    error.clear();
    ok &= Check(
        newScheduler.AbortGeneration(newHandle, error),
        "CURRENT_SCHEDULER_HANDLE_ABORTS_CURRENT_TRANSACTION");

    error.clear();
    ok &= Check(
        !newScheduler.AbortGeneration(newHandle, error),
        "ABORTED_TRANSACTION_HANDLE_CANNOT_BE_REUSED");

    return ok;
}



bool TestDrainInvalidatesPreviousIncarnationHandle()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-reset-handle", 0, 0.0, true);
    ok &= RegisterIdentity(scheduler, "ped-reset-handle", 6301);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    const auto oldPending = scheduler.GetPendingCandidates();
    if (oldPending.size() != 1 ||
        !oldPending.front().transactionHandle.has_value() ||
        !oldPending.front().decision.transactionId.has_value())
    {
        return Check(false, "DRAIN_HANDLE_FIXTURE_HAS_OLD_TRANSACTION");
    }

    const auto oldHandle = *oldPending.front().transactionHandle;
    const auto oldTransactionId = *oldPending.front().decision.transactionId;

    ok &= Check(
        scheduler.DrainTransactions(error) && error.empty(),
        "DRAIN_RESOLVES_OLD_PENDING_TRANSACTION");
    candidates.clear();
    report = {};
    error.clear();

    ok &= RunStep(
        scheduler, 50000, persons, subjects, context,
        candidates, report, error);

    const auto newPending = scheduler.GetPendingCandidates();
    if (newPending.size() != 1 ||
        !newPending.front().transactionHandle.has_value() ||
        !newPending.front().decision.transactionId.has_value())
    {
        return Check(false, "DRAIN_HANDLE_FIXTURE_HAS_NEW_TRANSACTION");
    }

    const auto newHandle = *newPending.front().transactionHandle;
    const auto newTransactionId = *newPending.front().decision.transactionId;

    ok &= Check(
        oldTransactionId == 1 && newTransactionId == 1 &&
            oldHandle.PersonId() == newHandle.PersonId() &&
            oldHandle.PresenceEpoch() == newHandle.PresenceEpoch(),
        "DRAIN_RESTARTS_LOCAL_ID_WITH_NEW_INCARNATION");

    ok &= Check(
        oldHandle.SchedulerInstanceId() != newHandle.SchedulerInstanceId(),
        "DRAIN_ROTATES_SCHEDULER_INCARNATION");

    error.clear();
    ok &= Check(
        !scheduler.ConfirmGeneration(oldHandle, 50, error),
        "OLD_INCARNATION_HANDLE_CANNOT_CONFIRM_AFTER_DRAIN");

    ok &= Check(
        scheduler.GetPendingCandidates().size() == 1,
        "DRAIN_STALE_HANDLE_DOES_NOT_MUTATE_NEW_PENDING_TRANSACTION");

    error.clear();
    ok &= Check(
        scheduler.AbortGeneration(newHandle, error),
        "NEW_HANDLE_ABORTS_AFTER_DRAIN");

    return ok;
}

bool TestOldManagerTransactionCannotResolveNewTransaction()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;
    VamGenerationScheduler scheduler(Config());

    bool ok = AddPerson(
        persons, subjects, "ped-stale-transaction", 0, 0.0, true);
    ok &= RegisterIdentity(scheduler, "ped-stale-transaction", 6401);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    auto pending = scheduler.GetPendingCandidates();
    if (pending.size() != 1 || !pending.front().transactionHandle.has_value())
    {
        return Check(false, "STALE_TRANSACTION_FIXTURE_HAS_FIRST_HANDLE");
    }
    const auto oldHandle = *pending.front().transactionHandle;

    error.clear();
    ok &= Check(
        scheduler.AbortGeneration(oldHandle, error),
        "FIRST_TRANSACTION_ABORTS_VALIDLY");

    candidates.clear();
    report = {};
    error.clear();
    ok &= RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    pending = scheduler.GetPendingCandidates();
    if (pending.size() != 1 || !pending.front().transactionHandle.has_value())
    {
        return Check(false, "STALE_TRANSACTION_FIXTURE_HAS_SECOND_HANDLE");
    }
    const auto newHandle = *pending.front().transactionHandle;

    ok &= Check(
        oldHandle.SchedulerInstanceId() == newHandle.SchedulerInstanceId() &&
            oldHandle.PersonId() == newHandle.PersonId() &&
            oldHandle.PresenceEpoch() == newHandle.PresenceEpoch() &&
            oldHandle.TransactionId() != newHandle.TransactionId(),
        "SAME_SCHEDULER_AND_PRESENCE_ISSUE_DISTINCT_TRANSACTION_HANDLE");

    error.clear();
    ok &= Check(
        !scheduler.ConfirmGeneration(oldHandle, 100, error),
        "OLD_MANAGER_TRANSACTION_CANNOT_CONFIRM_NEW_TRANSACTION");

    error.clear();
    ok &= Check(
        !scheduler.AbortGeneration(oldHandle, error),
        "OLD_MANAGER_TRANSACTION_CANNOT_ABORT_NEW_TRANSACTION");

    const auto afterStaleCalls = scheduler.GetPendingCandidates();
    ok &= Check(
        afterStaleCalls.size() == 1 &&
            afterStaleCalls.front().transactionHandle ==
                pending.front().transactionHandle,
        "STALE_MANAGER_HANDLE_DOES_NOT_MUTATE_CURRENT_PENDING_TRANSACTION");

    error.clear();
    ok &= Check(
        scheduler.AbortGeneration(newHandle, error),
        "CURRENT_MANAGER_TRANSACTION_HANDLE_REMAINS_USABLE");

    return ok;
}

bool TestTransactionHandleCannotBeForgedByCallers()
{
    using Handle = VamGenerationTransactionHandle;
    using SchedulerId = VamGenerationSchedulerInstanceId;
    using PresenceEpoch = VamPersonPresenceEpoch;

    return Check(
        !std::is_default_constructible_v<Handle> &&
            !std::is_constructible_v<
                Handle, SchedulerId, std::string, PresenceEpoch,
                std::uint64_t> &&
            std::is_copy_constructible_v<Handle>,
        "TRANSACTION_HANDLE_IS_OPAQUE_BUT_COPYABLE");
}

bool TestSchedulerInstanceIdentifierExhaustionFailsClosed()
{
    std::atomic<std::uint64_t> nextValue{
        std::numeric_limits<std::uint64_t>::max()};
    VamGenerationSchedulerInstanceId issued{17};

    const bool allocatedLastValue =
        detail::TryAllocateVamGenerationSchedulerInstanceId(
            nextValue, issued);

    bool ok = Check(
        allocatedLastValue &&
            issued.value == std::numeric_limits<std::uint64_t>::max() &&
            nextValue.load(std::memory_order_relaxed) == 0,
        "SCHEDULER_INSTANCE_ID_LAST_VALUE_IS_ALLOCATED");

    VamGenerationSchedulerInstanceId unchanged{99};
    const bool allocatedAfterExhaustion =
        detail::TryAllocateVamGenerationSchedulerInstanceId(
            nextValue, unchanged);

    ok &= Check(
        !allocatedAfterExhaustion && unchanged.value == 99 &&
            nextValue.load(std::memory_order_relaxed) == 0,
        "SCHEDULER_INSTANCE_ID_EXHAUSTION_NEVER_REUSES_VALUE");

    return ok;
}


using AbortExecutor = std::function<bool(
    VamGenerationManager&,
    std::uint64_t,
    std::string&)>;

template <typename Scheduler, typename Report>
constexpr bool HasRetiredTransactionApi()
{
    return std::is_constructible_v<
               Scheduler,
               VamGenerationSchedulerConfig,
               AbortExecutor> &&
        requires(
            Scheduler& scheduler,
            const std::string& personId,
            std::string& error,
            Report& report)
        {
            { scheduler.InvalidatePersonPresence(personId, error) }
                -> std::convertible_to<bool>;
            { scheduler.GetRetiredTransactionCount() }
                -> std::convertible_to<std::size_t>;
            { scheduler.IsPersonQuarantined(personId) }
                -> std::convertible_to<bool>;

            { report.retiredAbortAttempts }
                -> std::convertible_to<std::size_t>;
            { report.retiredAbortSuccesses }
                -> std::convertible_to<std::size_t>;
            { report.pendingRetiredTransactions }
                -> std::convertible_to<std::size_t>;
            { report.skippedQuarantinedSubjects }
                -> std::convertible_to<std::size_t>;
        };
}

template <typename Scheduler, typename Report>
bool TestU2RetiredTransactionLifecycle()
{
    if constexpr (!HasRetiredTransactionApi<Scheduler, Report>())
    {
        return Check(
            false,
            "U2_RETIREMENT_API_AND_ABORT_EXECUTOR_SEAM_AVAILABLE");
    }
    else
    {
        auto findCandidate =
            [](const std::vector<VamGenerationScheduledCandidate>& values,
               const std::string& personId)
                -> const VamGenerationScheduledCandidate*
            {
                for (const auto& candidate : values)
                {
                    if (candidate.egoPersonId == personId)
                    {
                        return &candidate;
                    }
                }
                return nullptr;
            };

        bool ok = true;

        // Accepted REMOVE must abort the selected pending transaction
        // immediately without disturbing another person's transaction.
        {
            auto& authority = ItsStationIdentityAuthority::Instance();
            authority.Clear();
            VehicleRegistry::Instance().Clear();
            SumoVehicleLifecycleRegistry::Instance().Clear();

            SumoPersonRegistry persons;
            SumoVruSubjectRegistry subjects;
            Scheduler scheduler(Config());

            ok &= AddPerson(persons, subjects, "retire-a", 0, 0.0, true);
            ok &= AddPerson(persons, subjects, "retire-b", 0, 10.0, true);
            ok &= RegisterIdentity(scheduler, "retire-a", 6001);
            ok &= RegisterIdentity(scheduler, "retire-b", 6002);

            VamGenerationContext context{};
            std::vector<VamGenerationScheduledCandidate> candidates;
            Report report{};
            std::string error;

            if (!RunStep(
                    scheduler, 0, persons, subjects, context,
                    candidates, report, error) ||
                candidates.size() != 2)
            {
                return Check(
                    false,
                    "U2_REMOVE_FIXTURE_HAS_TWO_PENDING_TRANSACTIONS");
            }

            const auto* removedCandidate =
                findCandidate(candidates, "retire-a");
            const auto* peerCandidate =
                findCandidate(candidates, "retire-b");

            if (!removedCandidate || !peerCandidate ||
                !removedCandidate->transactionHandle ||
                !peerCandidate->transactionHandle)
            {
                return Check(
                    false,
                    "U2_REMOVE_FIXTURE_HAS_IDENTITY_AWARE_HANDLES");
            }

            const auto removedHandle =
                *removedCandidate->transactionHandle;
            const auto peerHandle =
                *peerCandidate->transactionHandle;

            SumoPersonDatagram removal{};
            removal.eventType = SumoPersonEventType::Remove;
            removal.personId = "retire-a";
            removal.simulationTimeUs = 1000;

            const bool personRemoved = persons.Apply(removal);
            ok &= Check(
                personRemoved,
                "U2_REMOVE_ACCEPTED_BY_AUTHORITATIVE_PERSON_REGISTRY");

            error.clear();
            const bool invalidated =
                personRemoved &&
                scheduler.InvalidatePersonPresence("retire-a", error);

            ok &= Check(
                invalidated,
                "REMOVE_ABORTS_PENDING_TRANSACTION_IMMEDIATELY");

            ok &= Check(
                subjects.Remove("retire-a"),
                "U2_REMOVE_CLEARS_DERIVED_VRU_SUBJECT");

            const auto activePending = scheduler.GetPendingCandidates();

            ok &= Check(
                activePending.size() == 1 &&
                    activePending.front().egoPersonId == "retire-b",
                "REMOVE_RETIRES_ONLY_REMOVED_PERSON_TRANSACTION");

            error.clear();
            const bool lateConfirm =
                scheduler.ConfirmGeneration(removedHandle, 100, error);
            error.clear();
            const bool lateAbort =
                scheduler.AbortGeneration(removedHandle, error);

            ok &= Check(
                !lateConfirm && !lateAbort,
                "REMOVE_REJECTS_LATE_CONFIRMATION_AND_ABORT");

            const ItsStationIdentityOwner owner{
                ItsStationIdentityOwnerKind::ItsStation,
                "vam-vru:retire-a"
            };

            ok &= Check(
                authority.IsReservedBy(6001, owner),
                "REMOVE_PRESERVES_IDENTITY_RESERVATION");

            error.clear();
            ok &= Check(
                scheduler.AbortGeneration(peerHandle, error),
                "REMOVE_LEAVES_UNRELATED_TRANSACTION_RESOLVABLE");
        }

        // Failed aborts must retain the manager and handle, quarantine only
        // the affected person, and retry before the cadence early-return.
        {
            auto& authority = ItsStationIdentityAuthority::Instance();
            authority.Clear();
            VehicleRegistry::Instance().Clear();
            SumoVehicleLifecycleRegistry::Instance().Clear();

            SumoPersonRegistry persons;
            SumoVruSubjectRegistry subjects;

            std::size_t abortCalls = 0;
            AbortExecutor executor =
                [&abortCalls](
                    VamGenerationManager& manager,
                    std::uint64_t transactionId,
                    std::string& error)
                {
                    ++abortCalls;

                    // Three deterministic failures: REMOVE, cadence-skipped
                    // step, and the next full evaluation boundary.
                    if (abortCalls <= 3)
                    {
                        error = "injected retired-transaction abort failure";
                        return false;
                    }

                    return manager.AbortGeneration(transactionId, error);
                };

            Scheduler scheduler(Config(), executor);

            ok &= AddPerson(persons, subjects, "retire-a", 0, 0.0, true);
            ok &= AddPerson(persons, subjects, "retire-b", 0, 10.0, true);
            ok &= RegisterIdentity(scheduler, "retire-a", 6101);
            ok &= RegisterIdentity(scheduler, "retire-b", 6102);

            VamGenerationContext context{};
            std::vector<VamGenerationScheduledCandidate> candidates;
            Report report{};
            std::string error;

            if (!RunStep(
                    scheduler, 0, persons, subjects, context,
                    candidates, report, error) ||
                candidates.size() != 2)
            {
                return Check(
                    false,
                    "U2_RETRY_FIXTURE_HAS_TWO_PENDING_TRANSACTIONS");
            }

            const auto* candidateA =
                findCandidate(candidates, "retire-a");
            const auto* candidateB =
                findCandidate(candidates, "retire-b");

            if (!candidateA || !candidateB ||
                !candidateA->transactionHandle ||
                !candidateB->transactionHandle)
            {
                return Check(
                    false,
                    "U2_RETRY_FIXTURE_HAS_IDENTITY_AWARE_HANDLES");
            }

            const auto retiredHandle = *candidateA->transactionHandle;
            const auto peerHandle = *candidateB->transactionHandle;

            error.clear();
            ok &= Check(
                scheduler.ConfirmGeneration(peerHandle, 0, error),
                "U2_UNRELATED_PERSON_TRANSACTION_CONFIRMED");

            SumoPersonDatagram removal{};
            removal.eventType = SumoPersonEventType::Remove;
            removal.personId = "retire-a";
            removal.simulationTimeUs = 10;

            const bool personRemoved = persons.Apply(removal);
            ok &= Check(
                personRemoved,
                "U2_FAILED_ABORT_REMOVE_ACCEPTED");

            error.clear();
            const bool invalidated =
                personRemoved &&
                scheduler.InvalidatePersonPresence("retire-a", error);

            ok &= Check(
                !invalidated &&
                    scheduler.GetRetiredTransactionCount() == 1 &&
                    scheduler.IsPersonQuarantined("retire-a"),
                "FAILED_ABORT_PRESERVES_RETIRED_TRANSACTION");

            ok &= Check(
                subjects.Remove("retire-a"),
                "U2_FAILED_ABORT_CLEARS_DERIVED_SUBJECT");

            ok &= Check(
                scheduler.GetPendingCandidates().empty(),
                "FAILED_ABORT_HIDES_RETIRED_CANDIDATE_FROM_ACTIVE_PENDING_LIST");

            error.clear();
            const bool lateConfirm =
                scheduler.ConfirmGeneration(retiredHandle, 100, error);
            error.clear();
            const bool lateAbort =
                scheduler.AbortGeneration(retiredHandle, error);

            ok &= Check(
                !lateConfirm && !lateAbort,
                "RETIRED_HANDLE_REJECTS_LATE_RESOLUTION");

            // A new presence exists, but it cannot use the retired manager.
            ok &= AddPerson(
                persons, subjects, "retire-a", 20, 5.0, true);

            candidates.clear();
            report = {};
            error.clear();

            ok &= Check(
                RunStep(
                    scheduler, 50000, persons, subjects, context,
                    candidates, report, error),
                "U2_CADENCE_RETRY_STEP_ACCEPTED");

            ok &= Check(
                report.skippedByCadence &&
                    report.retiredAbortAttempts == 1 &&
                    report.pendingRetiredTransactions == 1 &&
                    scheduler.IsPersonQuarantined("retire-a") &&
                    abortCalls == 2,
                "RETIRED_RETRY_RUNS_BEFORE_CADENCE_RETURN");

            candidates.clear();
            report = {};
            error.clear();

            ok &= Check(
                RunStep(
                    scheduler, 100000, persons, subjects, context,
                    candidates, report, error),
                "U2_QUARANTINE_EVALUATION_STEP_ACCEPTED");

            const auto pendingWhileQuarantined =
                scheduler.GetPendingCandidates();

            const bool peerRemainsEligible =
                std::all_of(
                    pendingWhileQuarantined.begin(),
                    pendingWhileQuarantined.end(),
                    [](const auto& candidate)
                    {
                        return candidate.egoPersonId != "retire-a";
                    });

            ok &= Check(
                !report.skippedByCadence &&
                    report.retiredAbortAttempts == 1 &&
                    report.pendingRetiredTransactions == 1 &&
                    report.skippedQuarantinedSubjects == 1 &&
                    report.evaluatedSubjects == 1 &&
                    peerRemainsEligible &&
                    abortCalls == 3,
                "QUARANTINE_DOES_NOT_BLOCK_UNRELATED_SUBJECT");

            candidates.clear();
            report = {};
            error.clear();

            ok &= Check(
                RunStep(
                    scheduler, 200000, persons, subjects, context,
                    candidates, report, error),
                "U2_RETIRED_ABORT_RECOVERY_STEP_ACCEPTED");

            const auto* recoveredCandidate =
                findCandidate(candidates, "retire-a");

            ok &= Check(
                report.retiredAbortAttempts == 1 &&
                    report.retiredAbortSuccesses == 1 &&
                    report.pendingRetiredTransactions == 0 &&
                    scheduler.GetRetiredTransactionCount() == 0 &&
                    !scheduler.IsPersonQuarantined("retire-a") &&
                    abortCalls == 4,
                "RETIRED_ABORT_SUCCESS_RELEASES_QUARANTINE");

            ok &= Check(
                recoveredCandidate &&
                    recoveredCandidate->stationId.value == 6101 &&
                    recoveredCandidate->decision.reason ==
                        VamGenerationReason::InitialActivation &&
                    recoveredCandidate->transactionHandle.has_value() &&
                    *recoveredCandidate->transactionHandle != retiredHandle,
                "REENTRY_AFTER_RETIREMENT_CREATES_FRESH_INITIAL_MANAGER");

            error.clear();
            const bool staleConfirm =
                scheduler.ConfirmGeneration(retiredHandle, 200, error);
            error.clear();
            const bool staleAbort =
                scheduler.AbortGeneration(retiredHandle, error);

            ok &= Check(
                !staleConfirm && !staleAbort,
                "RETIRED_HANDLE_REMAINS_STALE_AFTER_RECOVERY");

            const ItsStationIdentityOwner owner{
                ItsStationIdentityOwnerKind::ItsStation,
                "vam-vru:retire-a"
            };

            ok &= Check(
                authority.IsReservedBy(6101, owner),
                "RETIRED_RECOVERY_PRESERVES_IDENTITY_RESERVATION");
        }

        return ok;
    }
}



template <typename Scheduler>
constexpr bool HasTransactionDrainApi()
{
    return requires(
        Scheduler& scheduler,
        std::string& error)
    {
        { scheduler.DrainTransactions(error) }
            -> std::convertible_to<bool>;
    };
}


bool TestU3RepeatedPartialDrainPreservesRetiredManager()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;

    std::size_t abortCalls = 0;
    bool allowRecovery = false;

    VamGenerationAbortExecutor abortExecutor =
        [&abortCalls, &allowRecovery](
            VamGenerationManager& manager,
            std::uint64_t transactionId,
            std::string& error)
        {
            const std::size_t call = ++abortCalls;

            // Drain 1: B and C fail.
            // Drain 2: C fails again after B has been resolved.
            // EvaluateStep: C remains failed until final recovery.
            const bool injectedFailure =
                !allowRecovery &&
                (call == 2 || call == 3 || call == 5 || call == 6);

            if (injectedFailure)
            {
                error = "injected repeated partial-drain failure";
                return false;
            }

            return manager.AbortGeneration(transactionId, error);
        };

    VamGenerationScheduler scheduler(Config(), abortExecutor);

    bool ok = AddPerson(
        persons, subjects, "ped-a", 0, 0.0, true);
    ok &= AddPerson(
        persons, subjects, "ped-b", 0, 10.0, true);
    ok &= AddPerson(
        persons, subjects, "ped-c", 0, 20.0, true);

    ok &= RegisterIdentity(scheduler, "ped-a", 6501);
    ok &= RegisterIdentity(scheduler, "ped-b", 6502);
    ok &= RegisterIdentity(scheduler, "ped-c", 6503);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    const auto initialPending = scheduler.GetPendingCandidates();
    const VamGenerationScheduledCandidate* initialB = nullptr;

    for (const auto& candidate : initialPending)
    {
        if (candidate.egoPersonId == "ped-b")
        {
            initialB = &candidate;
            break;
        }
    }

    if (!initialB || !initialB->transactionHandle.has_value())
    {
        return Check(
            false,
            "REPEATED_PARTIAL_DRAIN_FIXTURE_HAS_B_HANDLE");
    }

    const auto oldHandleB = *initialB->transactionHandle;

    ok &= Check(
        oldHandleB.TransactionId() == 1,
        "REPEATED_PARTIAL_DRAIN_ORIGINAL_B_USES_TRANSACTION_ONE");

    // First drain: A succeeds; B and C fail and enter the retired ledger.
    error.clear();
    const bool firstDrain = scheduler.DrainTransactions(error);

    ok &= Check(
        !firstDrain &&
            abortCalls == 3 &&
            scheduler.GetRetiredTransactionCount() == 2 &&
            scheduler.IsPersonQuarantined("ped-b") &&
            scheduler.IsPersonQuarantined("ped-c"),
        "REPEATED_PARTIAL_DRAIN_FIRST_ATTEMPT_PARTIAL");

    // Second drain: B succeeds, C fails again. B's resolved manager must
    // remain recoverable because the scheduler is still active.
    error.clear();
    const bool secondDrain = scheduler.DrainTransactions(error);

    ok &= Check(
        !secondDrain && abortCalls == 5,
        "REPEATED_PARTIAL_DRAIN_SECOND_ATTEMPT_PARTIAL");

    ok &= Check(
        scheduler.GetRetiredTransactionCount() == 1 &&
            !scheduler.IsPersonQuarantined("ped-b") &&
            scheduler.IsPersonQuarantined("ped-c"),
        "REPEATED_PARTIAL_DRAIN_RESTORES_RESOLVED_B_MANAGER");

    // A subsequent valid evaluation must restore B's original manager.
    candidates.clear();
    report = {};
    error.clear();

    const bool evaluated = RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        evaluated,
        "REPEATED_PARTIAL_DRAIN_EVALUATION_ACCEPTED");

    const auto afterEvaluation = scheduler.GetPendingCandidates();
    const VamGenerationScheduledCandidate* currentB = nullptr;

    for (const auto& candidate : afterEvaluation)
    {
        if (candidate.egoPersonId == "ped-b")
        {
            currentB = &candidate;
            break;
        }
    }

    if (!currentB ||
        !currentB->transactionHandle.has_value() ||
        !currentB->decision.transactionId.has_value())
    {
        return Check(
            false,
            "REPEATED_PARTIAL_DRAIN_B_HAS_NEW_CANDIDATE");
    }

    const auto newHandleB = *currentB->transactionHandle;

    ok &= Check(
        newHandleB.TransactionId() == 2,
        "REPEATED_PARTIAL_DRAIN_B_COUNTER_ADVANCES");

    const bool handlesAreDistinct = oldHandleB != newHandleB;

    ok &= Check(
        handlesAreDistinct,
        "REPEATED_PARTIAL_DRAIN_B_HANDLE_IS_DISTINCT");

    // Do not invoke a potentially aliased handle: the RED implementation
    // would accept it and mutate the new transaction.
    bool staleAbortRejected = false;
    bool staleConfirmationRejected = false;

    if (handlesAreDistinct)
    {
        error.clear();
        staleAbortRejected =
            !scheduler.AbortGeneration(oldHandleB, error);

        error.clear();
        staleConfirmationRejected =
            !scheduler.ConfirmGeneration(oldHandleB, 100, error);
    }

    ok &= Check(
        staleAbortRejected && staleConfirmationRejected,
        "REPEATED_PARTIAL_DRAIN_OLD_HANDLE_REJECTED");

    bool currentCandidatePreserved = false;

    for (const auto& candidate : scheduler.GetPendingCandidates())
    {
        if (candidate.egoPersonId == "ped-b" &&
            candidate.transactionHandle.has_value() &&
            *candidate.transactionHandle == newHandleB)
        {
            currentCandidatePreserved = true;
            break;
        }
    }

    ok &= Check(
        currentCandidatePreserved,
        "REPEATED_PARTIAL_DRAIN_NEW_CANDIDATE_PRESERVED");

    // Complete the remaining drain after allowing all aborts.
    allowRecovery = true;
    error.clear();

    ok &= Check(
        scheduler.DrainTransactions(error) &&
            scheduler.GetRetiredTransactionCount() == 0 &&
            scheduler.GetPendingCandidates().empty(),
        "REPEATED_PARTIAL_DRAIN_FINAL_DRAIN_COMPLETES");

    const auto& authority = ItsStationIdentityAuthority::Instance();

    const ItsStationIdentityOwner ownerA{
        ItsStationIdentityOwnerKind::ItsStation, "vam-vru:ped-a"};
    const ItsStationIdentityOwner ownerB{
        ItsStationIdentityOwnerKind::ItsStation, "vam-vru:ped-b"};
    const ItsStationIdentityOwner ownerC{
        ItsStationIdentityOwnerKind::ItsStation, "vam-vru:ped-c"};

    ok &= Check(
        authority.IsReservedBy(6501, ownerA) &&
            authority.IsReservedBy(6502, ownerB) &&
            authority.IsReservedBy(6503, ownerC),
        "REPEATED_PARTIAL_DRAIN_PRESERVES_IDENTITIES");

    return ok;
}

bool TestU3PartialDrainPreservesResolvedPeerManager()
{
    ItsStationIdentityAuthority::Instance().Clear();
    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    SumoPersonRegistry persons;
    SumoVruSubjectRegistry subjects;

    std::size_t abortCalls = 0;
    bool allowRecovery = false;

    VamGenerationAbortExecutor abortExecutor =
        [&abortCalls, &allowRecovery](
            VamGenerationManager& manager,
            std::uint64_t transactionId,
            std::string& abortError)
        {
            ++abortCalls;

            // Call 1 resolves ped-a during the first drain.
            // Calls 2 and 3 fail ped-b's initial and retired abort attempts.
            // Call 4 is ped-a's explicit follow-up abort and must succeed.
            // Call 5 recovers ped-b after allowRecovery is enabled.
            if (!allowRecovery &&
                (abortCalls == 2 || abortCalls == 3))
            {
                abortError = "injected abort failure for ped-b";
                return false;
            }

            return manager.AbortGeneration(transactionId, abortError);
        };

    VamGenerationScheduler scheduler(Config(), abortExecutor);

    bool ok = AddPerson(
        persons, subjects, "ped-a", 0, 0.0, true);
    ok &= AddPerson(
        persons, subjects, "ped-b", 0, 10.0, true);
    ok &= RegisterIdentity(scheduler, "ped-a", 6401);
    ok &= RegisterIdentity(scheduler, "ped-b", 6402);

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    ok &= RunStep(
        scheduler, 0, persons, subjects, context,
        candidates, report, error);

    const auto initialPending = scheduler.GetPendingCandidates();

    const VamGenerationScheduledCandidate* initialA = nullptr;
    const VamGenerationScheduledCandidate* initialB = nullptr;

    for (const auto& candidate : initialPending)
    {
        if (candidate.egoPersonId == "ped-a")
        {
            initialA = &candidate;
        }
        else if (candidate.egoPersonId == "ped-b")
        {
            initialB = &candidate;
        }
    }

    if (!initialA || !initialB ||
        !initialA->transactionHandle.has_value() ||
        !initialB->transactionHandle.has_value())
    {
        return Check(
            false,
            "PARTIAL_DRAIN_HANDLE_COLLISION_FIXTURE_READY");
    }

    const auto oldHandleA = *initialA->transactionHandle;
    const auto oldHandleB = *initialB->transactionHandle;

    ok &= Check(
        oldHandleA.TransactionId() == 1,
        "PARTIAL_DRAIN_ORIGINAL_PEER_USES_TRANSACTION_ONE");

    error.clear();

    const bool firstDrain =
        scheduler.DrainTransactions(error);

    ok &= Check(
        !firstDrain &&
            abortCalls == 2 &&
            scheduler.GetRetiredTransactionCount() == 1 &&
            scheduler.IsPersonQuarantined("ped-b"),
        "PARTIAL_DRAIN_FIXTURE_RETAINS_FAILED_PEER");

    // Retrying the retired abort still fails. ped-a must nevertheless
    // remain evaluable using its original manager.
    candidates.clear();
    report = {};
    error.clear();

    const bool reevaluated = RunStep(
        scheduler, 100000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        reevaluated,
        "PARTIAL_DRAIN_REEVALUATION_STEP_ACCEPTED");

    const auto newPending = scheduler.GetPendingCandidates();

    if (newPending.size() != 1 ||
        newPending.front().egoPersonId != "ped-a" ||
        !newPending.front().transactionHandle.has_value())
    {
        return Check(
            false,
            "PARTIAL_DRAIN_RESOLVED_PEER_REEVALUATES");
    }

    const auto newHandleA = *newPending.front().transactionHandle;

    ok &= Check(
        newHandleA.TransactionId() != oldHandleA.TransactionId(),
        "PARTIAL_DRAIN_RETRY_ADVANCES_LOCAL_TRANSACTION_ID");

    ok &= Check(
        newHandleA.SchedulerInstanceId() ==
                oldHandleA.SchedulerInstanceId() &&
            newHandleA.PersonId() == oldHandleA.PersonId() &&
            newHandleA.PresenceEpoch() == oldHandleA.PresenceEpoch(),
        "PARTIAL_DRAIN_RETRY_PRESERVES_SCHEDULER_AND_PRESENCE");

    error.clear();
    const bool staleConfirmationAccepted =
        scheduler.ConfirmGeneration(oldHandleA, 100, error);

    ok &= Check(
        !staleConfirmationAccepted,
        "PARTIAL_DRAIN_OLD_HANDLE_CANNOT_CONFIRM_NEW_TRANSACTION");

    error.clear();
    const bool staleAbortAccepted =
        scheduler.AbortGeneration(oldHandleA, error);

    ok &= Check(
        !staleAbortAccepted,
        "PARTIAL_DRAIN_OLD_HANDLE_CANNOT_ABORT_NEW_TRANSACTION");

    const auto pendingAfterStaleAttempts =
        scheduler.GetPendingCandidates();

    ok &= Check(
        pendingAfterStaleAttempts.size() == 1 &&
            pendingAfterStaleAttempts.front().transactionHandle.has_value() &&
            *pendingAfterStaleAttempts.front().transactionHandle == newHandleA,
        "PARTIAL_DRAIN_STALE_HANDLE_ATTEMPTS_PRESERVE_NEW_CANDIDATE");

    // Resolve A's follow-up candidate before retrying B.
    error.clear();
    ok &= Check(
        scheduler.AbortGeneration(newHandleA, error) && abortCalls == 4,
        "PARTIAL_DRAIN_RESOLVES_PEER_FOLLOWUP_CANDIDATE");

    // B remains active with the same SUMO presence epoch. Once its retired
    // abort succeeds, its original manager must be restored, not recreated.
    allowRecovery = true;
    candidates.clear();
    report = {};
    error.clear();

    const bool recoveryStep = RunStep(
        scheduler, 200000, persons, subjects, context,
        candidates, report, error);

    ok &= Check(
        recoveryStep && abortCalls == 5 &&
            !scheduler.IsPersonQuarantined("ped-b"),
        "PARTIAL_DRAIN_RECOVERY_RESTORES_SAME_PRESENCE_MANAGER");

    const auto recoveredPending = scheduler.GetPendingCandidates();
    const VamGenerationScheduledCandidate* recoveredB = nullptr;

    for (const auto& candidate : recoveredPending)
    {
        if (candidate.egoPersonId == "ped-b")
        {
            recoveredB = &candidate;
            break;
        }
    }

    if (!recoveredB || !recoveredB->transactionHandle.has_value())
    {
        return Check(
            false,
            "PARTIAL_DRAIN_RECOVERED_PEER_HAS_NEW_CANDIDATE");
    }

    const auto recoveredHandleB = *recoveredB->transactionHandle;

    ok &= Check(
        recoveredHandleB.TransactionId() != oldHandleB.TransactionId(),
        "PARTIAL_DRAIN_RECOVERY_ADVANCES_LOCAL_TRANSACTION_ID");

    error.clear();
    ok &= Check(
        !scheduler.AbortGeneration(oldHandleB, error),
        "PARTIAL_DRAIN_RETIRED_HANDLE_CANNOT_ABORT_RECOVERED_PRESENCE");

    error.clear();
    ok &= Check(
        !scheduler.ConfirmGeneration(oldHandleB, 200, error),
        "PARTIAL_DRAIN_RETIRED_HANDLE_CANNOT_CONFIRM_RECOVERED_PRESENCE");

    bool recoveredCandidateStillPending = false;
    for (const auto& candidate : scheduler.GetPendingCandidates())
    {
        if (candidate.egoPersonId == "ped-b" &&
            candidate.transactionHandle.has_value() &&
            *candidate.transactionHandle == recoveredHandleB)
        {
            recoveredCandidateStillPending = true;
        }
    }

    ok &= Check(
        recoveredCandidateStillPending,
        "PARTIAL_DRAIN_RECOVERY_STALE_HANDLE_PRESERVES_NEW_CANDIDATE");

    error.clear();
    ok &= Check(
        scheduler.DrainTransactions(error) &&
            scheduler.GetRetiredTransactionCount() == 0 &&
            scheduler.GetPendingCandidates().empty(),
        "PARTIAL_DRAIN_LATER_RETRY_COMPLETES");

    const ItsStationIdentityOwner ownerA{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:ped-a"
    };
    const ItsStationIdentityOwner ownerB{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:ped-b"
    };

    ok &= Check(
        ItsStationIdentityAuthority::Instance().IsReservedBy(6401, ownerA) &&
            ItsStationIdentityAuthority::Instance().IsReservedBy(6402, ownerB),
        "PARTIAL_DRAIN_PRESERVES_PEER_IDENTITY_RESERVATIONS");

    return ok;
}

template <typename Scheduler>
bool TestU3TransactionDrain()
{
    if constexpr (!HasTransactionDrainApi<Scheduler>())
    {
        return Check(
            false,
            "U3_TRANSACTION_DRAIN_API_AVAILABLE");
    }
    else
    {
        bool ok = Check(
            true,
            "U3_TRANSACTION_DRAIN_API_AVAILABLE");

        // A complete drain aborts pending transactions and leaves no active
        // or retired candidate behind.
        {
            auto& authority = ItsStationIdentityAuthority::Instance();
            authority.Clear();
            VehicleRegistry::Instance().Clear();
            SumoVehicleLifecycleRegistry::Instance().Clear();

            SumoPersonRegistry persons;
            SumoVruSubjectRegistry subjects;
            Scheduler scheduler(Config());

            ok &= AddPerson(
                persons, subjects, "drain-complete", 0, 0.0, true);
            ok &= RegisterIdentity(scheduler, "drain-complete", 6301);

            VamGenerationContext context{};
            std::vector<VamGenerationScheduledCandidate> candidates;
            VamGenerationSchedulerReport report{};
            std::string error;

            if (!RunStep(
                    scheduler, 0, persons, subjects, context,
                    candidates, report, error) ||
                candidates.size() != 1 ||
                !candidates.front().transactionHandle.has_value())
            {
                return Check(
                    false,
                    "U3_COMPLETE_DRAIN_FIXTURE_HAS_PENDING_HANDLE");
            }

            const auto oldHandle =
                *candidates.front().transactionHandle;

            error.clear();
            const bool drained = scheduler.DrainTransactions(error);

            ok &= Check(
                drained &&
                    scheduler.GetPendingCandidates().empty() &&
                    scheduler.GetRetiredTransactionCount() == 0 &&
                    !scheduler.IsPersonQuarantined("drain-complete"),
                "COMPLETE_DRAIN_RESOLVES_ALL_PENDING_TRANSACTIONS");

            error.clear();
            ok &= Check(
                !scheduler.ConfirmGeneration(oldHandle, 0, error),
                "COMPLETE_DRAIN_REJECTS_OLD_CONFIRMATION");

            error.clear();
            ok &= Check(
                !scheduler.AbortGeneration(oldHandle, error),
                "COMPLETE_DRAIN_REJECTS_OLD_ABORT");

            const ItsStationIdentityOwner owner{
                ItsStationIdentityOwnerKind::ItsStation,
                "vam-vru:drain-complete"
            };

            ok &= Check(
                authority.IsReservedBy(6301, owner),
                "COMPLETE_DRAIN_PRESERVES_ITS_IDENTITY_RESERVATION");

            error.clear();
            ok &= Check(
                scheduler.DrainTransactions(error),
                "COMPLETE_DRAIN_IS_IDEMPOTENT");
        }

        // A partial drain must retain failed transactions while keeping
        // successful aborts resolved. A later drain must recover the ledger.
        {
            auto& authority = ItsStationIdentityAuthority::Instance();
            authority.Clear();
            VehicleRegistry::Instance().Clear();
            SumoVehicleLifecycleRegistry::Instance().Clear();

            SumoPersonRegistry persons;
            SumoVruSubjectRegistry subjects;

            std::size_t abortCalls = 0;
            AbortExecutor executor =
                [&abortCalls](
                    VamGenerationManager& manager,
                    std::uint64_t transactionId,
                    std::string& error)
                {
                    ++abortCalls;

                    if (abortCalls <= 2)
                    {
                        error = "injected drain abort failure";
                        return false;
                    }

                    return manager.AbortGeneration(transactionId, error);
                };

            Scheduler scheduler(Config(), executor);

            ok &= AddPerson(
                persons, subjects, "drain-a", 0, 0.0, true);
            ok &= AddPerson(
                persons, subjects, "drain-b", 0, 10.0, true);
            ok &= RegisterIdentity(scheduler, "drain-a", 6311);
            ok &= RegisterIdentity(scheduler, "drain-b", 6312);

            VamGenerationContext context{};
            std::vector<VamGenerationScheduledCandidate> candidates;
            VamGenerationSchedulerReport report{};
            std::string error;

            if (!RunStep(
                    scheduler, 0, persons, subjects, context,
                    candidates, report, error) ||
                candidates.size() != 2)
            {
                return Check(
                    false,
                    "U3_PARTIAL_DRAIN_FIXTURE_HAS_TWO_CANDIDATES");
            }

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

            const auto* candidateA = findCandidate("drain-a");
            const auto* candidateB = findCandidate("drain-b");

            if (!candidateA || !candidateB ||
                !candidateA->transactionHandle ||
                !candidateB->transactionHandle)
            {
                return Check(
                    false,
                    "U3_PARTIAL_DRAIN_FIXTURE_HAS_IDENTITY_HANDLES");
            }

            const auto handleA = *candidateA->transactionHandle;
            const auto handleB = *candidateB->transactionHandle;

            // Establish one retired transaction before requesting the drain.
            error.clear();
            ok &= Check(
                !scheduler.InvalidatePersonPresence("drain-a", error) &&
                    scheduler.IsPersonQuarantined("drain-a"),
                "U3_PARTIAL_DRAIN_FIXTURE_RETAINS_FAILED_REMOVE_ABORT");

            error.clear();
            const bool firstDrain =
                scheduler.DrainTransactions(error);

            ok &= Check(
                !firstDrain &&
                    scheduler.GetRetiredTransactionCount() == 1 &&
                    scheduler.IsPersonQuarantined("drain-a") &&
                    !scheduler.IsPersonQuarantined("drain-b") &&
                    scheduler.GetPendingCandidates().empty() &&
                    abortCalls == 3,
                "PARTIAL_DRAIN_RETAINS_FAILED_ABORT_AND_RESOLVES_PEER");

            error.clear();
            ok &= Check(
                !scheduler.ConfirmGeneration(handleA, 100, error),
                "PARTIAL_DRAIN_REJECTS_RETIRED_CONFIRMATION");

            error.clear();
            ok &= Check(
                !scheduler.AbortGeneration(handleA, error),
                "PARTIAL_DRAIN_REJECTS_RETIRED_ABORT");

            error.clear();
            ok &= Check(
                !scheduler.ConfirmGeneration(handleB, 100, error) &&
                    !scheduler.AbortGeneration(handleB, error),
                "PARTIAL_DRAIN_KEEPS_SUCCESSFULLY_ABORTED_PEER_RESOLVED");

            const ItsStationIdentityOwner ownerA{
                ItsStationIdentityOwnerKind::ItsStation,
                "vam-vru:drain-a"
            };
            const ItsStationIdentityOwner ownerB{
                ItsStationIdentityOwnerKind::ItsStation,
                "vam-vru:drain-b"
            };

            ok &= Check(
                authority.IsReservedBy(6311, ownerA) &&
                    authority.IsReservedBy(6312, ownerB),
                "PARTIAL_DRAIN_PRESERVES_ALL_IDENTITY_RESERVATIONS");

            error.clear();
            const bool secondDrain =
                scheduler.DrainTransactions(error);

            ok &= Check(
                secondDrain &&
                    scheduler.GetRetiredTransactionCount() == 0 &&
                    !scheduler.IsPersonQuarantined("drain-a") &&
                    !scheduler.IsPersonQuarantined("drain-b") &&
                    scheduler.GetPendingCandidates().empty() &&
                    abortCalls == 4,
                "SECOND_DRAIN_RECOVERS_RETAINED_TRANSACTION");

            error.clear();
            ok &= Check(
                scheduler.DrainTransactions(error),
                "RECOVERED_DRAIN_IS_IDEMPOTENT");
        }

        return ok;
    }
}




template <typename Scheduler>
constexpr bool HasSubmittedTransactionApi()
{
    return requires(
        Scheduler& scheduler,
        const VamGenerationTransactionHandle& handle,
        std::string& error)
    {
        { scheduler.MarkGenerationSubmitted(handle, error) }
            -> std::convertible_to<bool>;

        { scheduler.CompleteGeneration(
              handle, true, std::int64_t{}, error) }
            -> std::convertible_to<bool>;

        { scheduler.GetInFlightTransactionCount() }
            -> std::convertible_to<std::size_t>;
    };
}


template <typename Scheduler>
bool TestSubmittedTransactionLifecycleForScheduler()
{
    if constexpr (!HasSubmittedTransactionApi<Scheduler>())
    {
        return Check(
            false,
            "VAM_SUBMITTED_TRANSACTION_API_AVAILABLE");
    }
    else
    {
        bool ok = Check(
            true,
            "VAM_SUBMITTED_TRANSACTION_API_AVAILABLE");

        // --------------------------------------------------------------
        // Fixture A: submit, REMOVE, reentry and late completion.
        // --------------------------------------------------------------
        {
            auto& authority = ItsStationIdentityAuthority::Instance();
            authority.Clear();
            VehicleRegistry::Instance().Clear();
            SumoVehicleLifecycleRegistry::Instance().Clear();

            SumoPersonRegistry persons;
            SumoVruSubjectRegistry subjects;
            Scheduler scheduler(Config());

            if (!AddPerson(
                    persons, subjects, "submitted-reentry", 0, 0.0, true) ||
                !RegisterIdentity(scheduler, "submitted-reentry", 7001))
            {
                return Check(
                    false,
                    "SUBMITTED_REENTRY_FIXTURE_INITIALIZED");
            }

            VamGenerationContext context{};
            std::vector<VamGenerationScheduledCandidate> candidates;
            VamGenerationSchedulerReport report{};
            std::string error;

            if (!RunStep(
                    scheduler, 0, persons, subjects, context,
                    candidates, report, error) ||
                candidates.size() != 1 ||
                !candidates.front().transactionHandle.has_value())
            {
                return Check(
                    false,
                    "SUBMITTED_REENTRY_FIXTURE_HAS_INITIAL_HANDLE");
            }

            const auto oldHandle =
                *candidates.front().transactionHandle;

            error.clear();
            const bool firstSubmit =
                scheduler.MarkGenerationSubmitted(oldHandle, error);

            error.clear();
            const bool duplicateSubmit =
                scheduler.MarkGenerationSubmitted(oldHandle, error);

            ok &= Check(
                firstSubmit &&
                    !duplicateSubmit &&
                    scheduler.GetInFlightTransactionCount() == 1,
                "MARK_SUBMITTED_IS_EXACTLY_ONCE");

            // Once the transport owns the operation, the legacy direct
            // resolution APIs must not bypass its terminal callback.
            error.clear();
            const bool directConfirm =
                scheduler.ConfirmGeneration(oldHandle, 0, error);

            error.clear();
            const bool directAbort =
                scheduler.AbortGeneration(oldHandle, error);

            ok &= Check(
                !directConfirm &&
                    !directAbort &&
                    scheduler.GetPendingCandidates().size() == 1 &&
                    scheduler.GetInFlightTransactionCount() == 1,
                "SUBMITTED_DIRECT_CONFIRM_ABORT_REJECTED");

            SumoPersonDatagram removal{};
            removal.eventType = SumoPersonEventType::Remove;
            removal.personId = "submitted-reentry";
            removal.simulationTimeUs = 50000;

            const bool personRemoved = persons.Apply(removal);

            error.clear();
            const bool presenceInvalidated =
                personRemoved &&
                scheduler.InvalidatePersonPresence(
                    "submitted-reentry", error);

            const bool subjectRemoved =
                subjects.Remove("submitted-reentry");

            ok &= Check(
                personRemoved &&
                    presenceInvalidated &&
                    subjectRemoved &&
                    scheduler.GetPendingCandidates().empty() &&
                    scheduler.GetInFlightTransactionCount() == 1 &&
                    !scheduler.IsPersonQuarantined("submitted-reentry"),
                "REMOVE_RETIRES_SUBMITTED_WITHOUT_ABORT");

            // Reentry keeps the logical station identity but creates a
            // distinct presence and generation manager.
            ok &= Check(
                AddPerson(
                    persons, subjects, "submitted-reentry",
                    200000, 5.0, true),
                "SUBMITTED_REENTRY_ACCEPTED");

            candidates.clear();
            report = {};
            error.clear();

            const bool reentryEvaluated =
                RunStep(
                    scheduler, 200000, persons, subjects, context,
                    candidates, report, error);

            if (!reentryEvaluated ||
                candidates.size() != 1 ||
                !candidates.front().transactionHandle.has_value())
            {
                return Check(
                    false,
                    "SUBMITTED_REENTRY_HAS_NEW_PENDING_HANDLE");
            }

            const auto newHandle =
                *candidates.front().transactionHandle;

            ok &= Check(
                oldHandle.PersonId() == newHandle.PersonId() &&
                    oldHandle.SchedulerInstanceId() ==
                        newHandle.SchedulerInstanceId() &&
                    oldHandle.PresenceEpoch() !=
                        newHandle.PresenceEpoch() &&
                    oldHandle != newHandle &&
                    candidates.front().stationId.value == 7001,
                "REENTRY_COEXISTS_WITH_OLD_IN_FLIGHT_HANDLE");

            // The old completion must resolve only the old manager.
            error.clear();
            const bool oldCompleted =
                scheduler.CompleteGeneration(
                    oldHandle, true, 200, error);

            const auto pendingAfterOldCompletion =
                scheduler.GetPendingCandidates();

            ok &= Check(
                oldCompleted &&
                    scheduler.GetInFlightTransactionCount() == 0 &&
                    pendingAfterOldCompletion.size() == 1 &&
                    pendingAfterOldCompletion.front().transactionHandle ==
                        std::optional<VamGenerationTransactionHandle>{
                            newHandle},
                "LATE_COMPLETION_RESOLVES_ONLY_OLD_PRESENCE");

            error.clear();
            const bool duplicateCompletion =
                scheduler.CompleteGeneration(
                    oldHandle, true, 200, error);

            const auto pendingAfterDuplicate =
                scheduler.GetPendingCandidates();

            ok &= Check(
                !duplicateCompletion &&
                    pendingAfterDuplicate.size() == 1 &&
                    pendingAfterDuplicate.front().transactionHandle ==
                        std::optional<VamGenerationTransactionHandle>{
                            newHandle},
                "DUPLICATE_COMPLETION_CANNOT_MUTATE_REENTRY");

            // A terminal transport failure aborts the new manager's
            // candidate instead of committing its generation baseline.
            error.clear();
            const bool newSubmitted =
                scheduler.MarkGenerationSubmitted(newHandle, error);

            error.clear();
            const bool newFailed =
                scheduler.CompleteGeneration(
                    newHandle, false, 200, error);

            ok &= Check(
                newSubmitted &&
                    newFailed &&
                    scheduler.GetInFlightTransactionCount() == 0 &&
                    scheduler.GetPendingCandidates().empty(),
                "FAILED_SUBMISSION_ABORTS_GENERATION");
        }

        // --------------------------------------------------------------
        // Fixture B: drain must not abort an active submitted candidate.
        // --------------------------------------------------------------
        {
            auto& authority = ItsStationIdentityAuthority::Instance();
            authority.Clear();
            VehicleRegistry::Instance().Clear();
            SumoVehicleLifecycleRegistry::Instance().Clear();

            SumoPersonRegistry persons;
            SumoVruSubjectRegistry subjects;
            Scheduler scheduler(Config());

            if (!AddPerson(
                    persons, subjects, "submitted-drain", 0, 0.0, true) ||
                !RegisterIdentity(scheduler, "submitted-drain", 7002))
            {
                return Check(
                    false,
                    "SUBMITTED_DRAIN_FIXTURE_INITIALIZED");
            }

            VamGenerationContext context{};
            std::vector<VamGenerationScheduledCandidate> candidates;
            VamGenerationSchedulerReport report{};
            std::string error;

            if (!RunStep(
                    scheduler, 0, persons, subjects, context,
                    candidates, report, error) ||
                candidates.size() != 1 ||
                !candidates.front().transactionHandle.has_value())
            {
                return Check(
                    false,
                    "SUBMITTED_DRAIN_FIXTURE_HAS_PENDING_HANDLE");
            }

            const auto handle =
                *candidates.front().transactionHandle;

            error.clear();
            const bool submitted =
                scheduler.MarkGenerationSubmitted(handle, error);

            error.clear();
            const bool prematureDrain =
                scheduler.DrainTransactions(error);

            const auto pendingAfterDrain =
                scheduler.GetPendingCandidates();

            ok &= Check(
                submitted &&
                    !prematureDrain &&
                    scheduler.GetInFlightTransactionCount() == 1 &&
                    pendingAfterDrain.size() == 1 &&
                    pendingAfterDrain.front().transactionHandle ==
                        std::optional<VamGenerationTransactionHandle>{
                            handle},
                "DRAIN_BLOCKS_WITHOUT_ABORTING_SUBMITTED_TRANSACTION");

            // Resolve the actual transport operation, then retry shutdown.
            error.clear();
            const bool completed =
                scheduler.CompleteGeneration(
                    handle, true, 100, error);

            error.clear();
            const bool finalDrain =
                scheduler.DrainTransactions(error);

            ok &= Check(
                completed &&
                    finalDrain &&
                    scheduler.GetInFlightTransactionCount() == 0 &&
                    scheduler.GetPendingCandidates().empty(),
                "DRAIN_COMPLETES_AFTER_TERMINAL_CALLBACK");

            error.clear();
            ok &= Check(
                !scheduler.ConfirmGeneration(handle, 100, error),
                "DRAIN_INVALIDATES_COMPLETED_SUBMITTED_HANDLE");
        }

        return ok;
    }
}


bool TestPresenceRuntimeOwnershipQuery()
{
    using namespace vanetza_integration::vam;

    bool ok = true;

    // An active runtime belongs only to its exact SUMO presence epoch.
    {
        ItsStationIdentityAuthority::Instance().Clear();
        VehicleRegistry::Instance().Clear();
        SumoVehicleLifecycleRegistry::Instance().Clear();

        SumoPersonRegistry persons;
        SumoVruSubjectRegistry subjects;
        VamGenerationScheduler scheduler(Config());
        VamGenerationContext context{};
        std::vector<VamGenerationScheduledCandidate> candidates;
        VamGenerationSchedulerReport report{};
        std::string error;

        if (!AddPerson(persons, subjects, "presence-query-active", 0, 0.0, true) ||
            !RegisterIdentity(scheduler, "presence-query-active", 7801) ||
            !RunStep(scheduler, 0, persons, subjects, context, candidates, report, error) ||
            candidates.size() != 1 ||
            !candidates.front().transactionHandle.has_value())
        {
            return Check(false, "PRESENCE_QUERY_ACTIVE_FIXTURE_READY");
        }

        const VamPersonPresenceEpoch activeEpoch{0};
        ok &= Check(
            scheduler.HasRetainedRuntimeForPresence("presence-query-active", activeEpoch) &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-active", VamPersonPresenceEpoch{1}),
            "ACTIVE_RUNTIME_LOOKUP_MATCHES_EXACT_PRESENCE_EPOCH");

        SumoPersonDatagram removal{};
        removal.eventType = SumoPersonEventType::Remove;
        removal.personId = "presence-query-active";
        removal.simulationTimeUs = 1;
        const bool removed = persons.Apply(removal);
        error.clear();
        const bool invalidated = removed &&
            scheduler.InvalidatePersonPresence("presence-query-active", error);

        ok &= Check(
            invalidated &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-active", activeEpoch),
            "SUCCESSFUL_REMOVE_ABORT_RELEASES_ACTIVE_PRESENCE_RUNTIME");
    }

    // A failed REMOVE abort is retained in the retired ledger and remains
    // discoverable until a later drain successfully resolves it.
    {
        ItsStationIdentityAuthority::Instance().Clear();
        VehicleRegistry::Instance().Clear();
        SumoVehicleLifecycleRegistry::Instance().Clear();

        SumoPersonRegistry persons;
        SumoVruSubjectRegistry subjects;
        bool allowAbort = false;
        VamGenerationAbortExecutor abortExecutor =
            [&allowAbort](
                VamGenerationManager& manager,
                std::uint64_t transactionId,
                std::string& abortError)
            {
                if (!allowAbort)
                {
                    abortError = "injected presence-query abort failure";
                    return false;
                }
                return manager.AbortGeneration(transactionId, abortError);
            };
        VamGenerationScheduler scheduler(Config(), abortExecutor);
        VamGenerationContext context{};
        std::vector<VamGenerationScheduledCandidate> candidates;
        VamGenerationSchedulerReport report{};
        std::string error;

        if (!AddPerson(persons, subjects, "presence-query-retired", 0, 0.0, true) ||
            !RegisterIdentity(scheduler, "presence-query-retired", 7802) ||
            !RunStep(scheduler, 0, persons, subjects, context, candidates, report, error) ||
            candidates.size() != 1)
        {
            return Check(false, "PRESENCE_QUERY_RETIRED_FIXTURE_READY");
        }

        SumoPersonDatagram removal{};
        removal.eventType = SumoPersonEventType::Remove;
        removal.personId = "presence-query-retired";
        removal.simulationTimeUs = 1;
        const bool removed = persons.Apply(removal);
        error.clear();
        const bool invalidated = removed &&
            scheduler.InvalidatePersonPresence("presence-query-retired", error);

        const VamPersonPresenceEpoch oldEpoch{0};
        ok &= Check(
            !invalidated &&
                scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-retired", oldEpoch) &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-retired", VamPersonPresenceEpoch{2}),
            "FAILED_ABORT_REMAINS_RETAINED_ONLY_FOR_OLD_PRESENCE");

        allowAbort = true;
        error.clear();
        const bool drained = scheduler.DrainTransactions(error);
        ok &= Check(
            drained &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-retired", oldEpoch),
            "RETIRED_ABORT_RECOVERY_RELEASES_OLD_PRESENCE_RUNTIME");
    }

    // A submitted transaction moved to the full-handle in-flight ledger
    // remains attached to the old epoch while a new epoch becomes active.
    {
        ItsStationIdentityAuthority::Instance().Clear();
        VehicleRegistry::Instance().Clear();
        SumoVehicleLifecycleRegistry::Instance().Clear();

        SumoPersonRegistry persons;
        SumoVruSubjectRegistry subjects;
        VamGenerationScheduler scheduler(Config());
        VamGenerationContext context{};
        std::vector<VamGenerationScheduledCandidate> candidates;
        VamGenerationSchedulerReport report{};
        std::string error;

        if (!AddPerson(persons, subjects, "presence-query-inflight", 0, 0.0, true) ||
            !RegisterIdentity(scheduler, "presence-query-inflight", 7803) ||
            !RunStep(scheduler, 0, persons, subjects, context, candidates, report, error) ||
            candidates.size() != 1 ||
            !candidates.front().transactionHandle.has_value())
        {
            return Check(false, "PRESENCE_QUERY_INFLIGHT_FIXTURE_READY");
        }

        const auto oldHandle = *candidates.front().transactionHandle;
        error.clear();
        const bool submitted = scheduler.MarkGenerationSubmitted(oldHandle, error);

        SumoPersonDatagram removal{};
        removal.eventType = SumoPersonEventType::Remove;
        removal.personId = "presence-query-inflight";
        removal.simulationTimeUs = 1;
        const bool removed = persons.Apply(removal);
        error.clear();
        const bool invalidated = removed &&
            scheduler.InvalidatePersonPresence("presence-query-inflight", error);
        const bool subjectRemoved = subjects.Remove("presence-query-inflight");
        ok &= Check(
            subjectRemoved,
            "PRESENCE_QUERY_REMOVE_CLEARS_ACTIVE_SUBJECT");

        const VamPersonPresenceEpoch oldEpoch{0};
        const VamPersonPresenceEpoch newEpoch{200000};
        ok &= Check(
            submitted && invalidated && subjectRemoved &&
                scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", oldEpoch) &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", newEpoch),
            "SUBMITTED_REMOVE_RETAINS_EXACT_OLD_PRESENCE_RUNTIME");

        if (!AddPerson(persons, subjects, "presence-query-inflight", 200000, 5.0, true))
        {
            return Check(false, "PRESENCE_QUERY_REENTRY_FIXTURE_READY");
        }
        candidates.clear();
        report = {};
        error.clear();
        if (!RunStep(scheduler, 200000, persons, subjects, context, candidates, report, error) ||
            candidates.size() != 1 ||
            !candidates.front().transactionHandle.has_value())
        {
            return Check(false, "PRESENCE_QUERY_REENTRY_HAS_NEW_HANDLE");
        }

        const auto newHandle = *candidates.front().transactionHandle;
        ok &= Check(
            oldHandle.PresenceEpoch() != newHandle.PresenceEpoch() &&
                scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", oldEpoch) &&
                scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", newEpoch),
            "OLD_INFLIGHT_AND_NEW_ACTIVE_PRESENCES_COEXIST");

        error.clear();
        const bool oldCompleted = scheduler.CompleteGeneration(oldHandle, true, 200, error);
        ok &= Check(
            oldCompleted &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", oldEpoch) &&
                scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", newEpoch),
            "OLD_COMPLETION_RELEASES_ONLY_OLD_PRESENCE_RUNTIME");

        error.clear();
        const bool newAborted = scheduler.AbortGeneration(newHandle, error);
        ok &= Check(
            newAborted &&
                scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", newEpoch),
            "NEW_PRESENCE_RUNTIME_REMAINS_ACTIVE_AFTER_CANDIDATE_ABORT");

        // Aborting an individual candidate must preserve the active manager.
        // The presence runtime is released only when that presence is invalidated.
        error.clear();
        const bool newInvalidated =
            scheduler.InvalidatePersonPresence("presence-query-inflight", error);
        ok &= Check(
            newInvalidated &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-inflight", newEpoch),
            "NEW_PRESENCE_RUNTIME_RELEASED_AFTER_ITS_OWN_REMOVE");
    }

    // A terminal transport result is still retained if manager resolution
    // fails. Repeating the identical completion after recovery releases it.
    {
        ItsStationIdentityAuthority::Instance().Clear();
        VehicleRegistry::Instance().Clear();
        SumoVehicleLifecycleRegistry::Instance().Clear();

        SumoPersonRegistry persons;
        SumoVruSubjectRegistry subjects;
        bool allowAbort = false;
        VamGenerationAbortExecutor abortExecutor =
            [&allowAbort](
                VamGenerationManager& manager,
                std::uint64_t transactionId,
                std::string& abortError)
            {
                if (!allowAbort)
                {
                    abortError = "injected terminal-resolution failure";
                    return false;
                }
                return manager.AbortGeneration(transactionId, abortError);
            };
        VamGenerationScheduler scheduler(Config(), abortExecutor);
        VamGenerationContext context{};
        std::vector<VamGenerationScheduledCandidate> candidates;
        VamGenerationSchedulerReport report{};
        std::string error;

        if (!AddPerson(persons, subjects, "presence-query-terminal", 0, 0.0, true) ||
            !RegisterIdentity(scheduler, "presence-query-terminal", 7804) ||
            !RunStep(scheduler, 0, persons, subjects, context, candidates, report, error) ||
            candidates.size() != 1 ||
            !candidates.front().transactionHandle.has_value())
        {
            return Check(false, "PRESENCE_QUERY_TERMINAL_FIXTURE_READY");
        }

        const auto handle = *candidates.front().transactionHandle;
        error.clear();
        const bool submitted = scheduler.MarkGenerationSubmitted(handle, error);

        SumoPersonDatagram removal{};
        removal.eventType = SumoPersonEventType::Remove;
        removal.personId = "presence-query-terminal";
        removal.simulationTimeUs = 1;
        const bool removed = persons.Apply(removal);
        error.clear();
        const bool invalidated = removed &&
            scheduler.InvalidatePersonPresence("presence-query-terminal", error);

        error.clear();
        const bool firstCompletion =
            scheduler.CompleteGeneration(handle, false, 1, error);
        const VamPersonPresenceEpoch epoch{0};
        ok &= Check(
            submitted && invalidated && !firstCompletion &&
                scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-terminal", epoch),
            "TERMINAL_RESULT_WITH_FAILED_MANAGER_RESOLUTION_REMAINS_RETAINED");

        allowAbort = true;
        error.clear();
        const bool retriedCompletion =
            scheduler.CompleteGeneration(handle, false, 1, error);
        ok &= Check(
            retriedCompletion &&
                !scheduler.HasRetainedRuntimeForPresence(
                    "presence-query-terminal", epoch),
            "RETRIED_TERMINAL_RESOLUTION_RELEASES_RETAINED_RUNTIME");
    }

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
    ok &= TestStalePresenceTransactionCannotConfirmReentry();
    ok &= TestOldSchedulerTransactionCannotAbortNewScheduler();
    ok &= TestDrainInvalidatesPreviousIncarnationHandle();
    ok &= TestOldManagerTransactionCannotResolveNewTransaction();
    ok &= TestTransactionHandleCannotBeForgedByCallers();
    ok &= TestSchedulerInstanceIdentifierExhaustionFailsClosed();
    ok &= TestU2RetiredTransactionLifecycle<VamGenerationScheduler, VamGenerationSchedulerReport>();
    ok &= TestU3TransactionDrain<VamGenerationScheduler>();
    ok &= TestU3PartialDrainPreservesResolvedPeerManager();
    ok &= TestU3RepeatedPartialDrainPreservesRetiredManager();

    const bool presenceRuntimeOwnershipOk =
        TestPresenceRuntimeOwnershipQuery();
    ok &= presenceRuntimeOwnershipOk;

    const bool submittedLifecycleOk =
        TestSubmittedTransactionLifecycleForScheduler<VamGenerationScheduler>();
    ok &= submittedLifecycleOk;


    std::cout << "VAM_PRESENCE_RUNTIME_OWNERSHIP="
              << (presenceRuntimeOwnershipOk ? "PASS" : "FAIL") << '\n';
    std::cout << "VAM_GENERATION_SCHEDULER="
              << (ok ? "PASS" : "FAIL") << '\n';
    std::cout << "VAM_TRANSACTION_LIFECYCLE="
              << (ok ? "PASS" : "FAIL") << '\n';
    std::cout << "VAM_TRANSACTION_RETIREMENT="
              << (ok ? "PASS" : "FAIL") << '\n';
    std::cout << "VAM_TRANSACTION_DRAIN="
              << (ok ? "PASS" : "FAIL") << '\n';
    std::cout << "VAM_SUBMITTED_TRANSACTION_LIFECYCLE="
              << (submittedLifecycleOk ? "PASS" : "FAIL") << '\n';

    return ok ? 0 : 1;
}
