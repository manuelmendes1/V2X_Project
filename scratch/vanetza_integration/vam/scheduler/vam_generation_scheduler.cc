#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"

#include "vanetza_integration/vam/poti/vam_poti_sample.h"
#include "vanetza_integration/vam/poti/vam_poti_temporal_transform.h"

#include <chrono>

#include <atomic>
#include <chrono>
#include <limits>
#include <tuple>
#include <utility>

#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/vru/role/policy/vru_role_policy.h"

namespace vanetza_integration
{
namespace vam
{

namespace
{

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}


std::optional<VamGenerationSchedulerInstanceId>
AllocateSchedulerInstanceId() noexcept
{
    // The counter is process-wide and is never reset with scheduler runtime.
    static std::atomic<std::uint64_t> nextValue{1};
    VamGenerationSchedulerInstanceId identity{};

    if (!detail::TryAllocateVamGenerationSchedulerInstanceId(
            nextValue, identity))
    {
        return std::nullopt;
    }

    return identity;
}

bool ComposeItsTime(
    std::int64_t itsStartClockUs,
    std::uint64_t simulationTimeUs,
    std::chrono::microseconds& output,
    std::string& error,
    const char* fieldName)
{
    constexpr auto max =
        std::numeric_limits<std::int64_t>::max();

    if (simulationTimeUs >
        static_cast<std::uint64_t>(max))
    {
        error = std::string(fieldName) +
                " exceeds the ITS clock representation";
        return false;
    }

    const auto delta =
        static_cast<std::int64_t>(simulationTimeUs);

    if (itsStartClockUs > max - delta)
    {
        error = std::string(fieldName) +
                " overflows the ITS clock";
        return false;
    }

    output = std::chrono::microseconds{
        itsStartClockUs + delta
    };

    return true;
}

} // namespace

VamGenerationScheduler::SubjectRuntime::SubjectRuntime(
    const VamGenerationConfig& generationConfig,
    VamPersonPresenceEpoch presenceEpoch)
    : presenceEpoch(presenceEpoch),
      generationManager(generationConfig)
{
}

VamGenerationScheduler::VamGenerationScheduler(
    VamGenerationSchedulerConfig config,
    VamGenerationAbortExecutor abortExecutor)
    : m_config(std::move(config)),
      m_schedulerInstanceId(AllocateSchedulerInstanceId()),
      m_abortExecutor(std::move(abortExecutor))
{
    if (!m_abortExecutor)
    {
        m_abortExecutor = [](
            VamGenerationManager& manager,
            std::uint64_t transactionId,
            std::string& error)
        {
            return manager.AbortGeneration(transactionId, error);
        };
    }
}


bool
VamGenerationScheduler::RegisterSubjectStationIdentity(
    const std::string& personId,
    VamStationId stationId,
    std::string& error)
{
    if (personId.empty())
    {
        return Reject(
            error,
            "VAM subject personId must not be empty");
    }

    if (stationId.value == 0)
    {
        return Reject(
            error,
            "VAM subject stationId must be non-zero");
    }

    const ns3::ItsStationIdentityOwner owner{
        ns3::ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:" + personId
    };

    auto& authority =
        ns3::ItsStationIdentityAuthority::Instance();

    if (!authority.Reserve(owner, stationId.value))
    {
        return Reject(
            error,
            "VAM subject stationId reservation failed or conflicts with an existing reservation");
    }

    error.clear();
    return true;
}

bool
VamGenerationScheduler::AbortRuntimeCandidate(
    SubjectRuntime& runtime,
    const std::string& personId,
    bool requireCurrentSchedulerInstance,
    std::string& error)
{
    if (!runtime.pendingCandidate.has_value())
    {
        return Reject(
            error,
            "Cannot abort a VAM runtime without a pending candidate");
    }

    if (runtime.submitted)
    {
        return Reject(
            error,
            "Cannot abort a VAM candidate already submitted to transport");
    }

    const auto& candidate = *runtime.pendingCandidate;

    if (candidate.egoPersonId != personId ||
        !candidate.transactionHandle.has_value() ||
        !candidate.decision.transactionId.has_value())
    {
        return Reject(
            error,
            "Retired VAM candidate is missing consistent transaction identity");
    }

    const auto& handle = *candidate.transactionHandle;
    const auto handleSchedulerId = handle.SchedulerInstanceId();

    if (handle.PersonId() != personId ||
        handle.PresenceEpoch() != runtime.presenceEpoch ||
        handle.TransactionId() == 0 ||
        handleSchedulerId.value == 0 ||
        *candidate.decision.transactionId != handle.TransactionId())
    {
        return Reject(
            error,
            "VAM candidate handle does not match its retained runtime");
    }

    if (requireCurrentSchedulerInstance &&
        (!m_schedulerInstanceId.has_value() ||
         handleSchedulerId != *m_schedulerInstanceId))
    {
        return Reject(
            error,
            "VAM candidate does not belong to the current scheduler instance");
    }

    // The executor contract requires false to leave the manager transaction
    // pending and recoverable. Production delegates to AbortGeneration.
    if (!m_abortExecutor(
            runtime.generationManager,
            handle.TransactionId(),
            error))
    {
        return false;
    }

    runtime.pendingCandidate.reset();
    error.clear();
    return true;
}

bool
VamGenerationScheduler::InvalidatePersonPresence(
    const std::string& personId,
    std::string& error)
{
    if (personId.empty())
    {
        return Reject(error, "VAM personId must not be empty");
    }

    // Repeated invalidation is idempotent. A retired transaction is retried
    // only at the next valid EvaluateStep boundary.
    if (m_retiredRuntimeByPerson.find(personId) !=
        m_retiredRuntimeByPerson.end())
    {
        error.clear();
        return true;
    }

    const auto runtimeIt = m_subjectRuntimeByPerson.find(personId);
    if (runtimeIt == m_subjectRuntimeByPerson.end())
    {
        error.clear();
        return true;
    }

    if (!runtimeIt->second.pendingCandidate.has_value())
    {
        m_subjectRuntimeByPerson.erase(runtimeIt);
        error.clear();
        return true;
    }

    if (runtimeIt->second.submitted)
    {
        const auto& candidate = *runtimeIt->second.pendingCandidate;
        if (!candidate.transactionHandle.has_value())
        {
            return Reject(
                error,
                "Submitted VAM candidate has no transaction handle");
        }

        const auto key = MakeSubmittedTransactionKey(
            *candidate.transactionHandle);
        if (m_inFlightRuntimeByHandle.find(key) !=
            m_inFlightRuntimeByHandle.end())
        {
            return Reject(
                error,
                "Submitted VAM transaction already exists in the in-flight ledger");
        }

        m_inFlightRuntimeByHandle.emplace(
            key,
            std::move(runtimeIt->second));
        m_subjectRuntimeByPerson.erase(runtimeIt);
        error.clear();
        return true;
    }

    std::string abortError;
    if (AbortRuntimeCandidate(
            runtimeIt->second,
            personId,
            true,
            abortError))
    {
        m_subjectRuntimeByPerson.erase(runtimeIt);
        error.clear();
        return true;
    }

    // Fail closed: retain the manager, proximity history, candidate and
    // identity-aware handle. The ledger membership quarantines this personId.
    const auto [retiredIt, inserted] =
        m_retiredRuntimeByPerson.emplace(
            personId,
            std::move(runtimeIt->second));
    (void)retiredIt;

    if (!inserted)
    {
        return Reject(
            error,
            "VAM person already has a retired transaction");
    }

    m_subjectRuntimeByPerson.erase(runtimeIt);

    error = abortError.empty()
        ? "VAM candidate abort failed; transaction retained in quarantine"
        : std::move(abortError);
    return false;
}

void
VamGenerationScheduler::RetryRetiredTransactions(
    const std::map<std::string, VamPersonPresenceEpoch>& activePresenceByPerson,
    VamGenerationSchedulerReport& report)
{
    for (auto it = m_retiredRuntimeByPerson.begin();
         it != m_retiredRuntimeByPerson.end();)
    {
        // A malformed ledger entry is retained fail-closed.
        if (!it->second.pendingCandidate.has_value())
        {
            ++it;
            continue;
        }

        ++report.retiredAbortAttempts;

        std::string abortError;
        if (!AbortRuntimeCandidate(
                it->second,
                it->first,
                false,
                abortError))
        {
            ++it;
            continue;
        }

        ++report.retiredAbortSuccesses;

        // A successful retry must restore the original manager when the
        // same SUMO presence is still active. This preserves its local
        // transaction counter and proximity history.
        const auto presenceIt = activePresenceByPerson.find(it->first);
        const bool samePresence =
            presenceIt != activePresenceByPerson.end() &&
            presenceIt->second == it->second.presenceEpoch;

        if (!samePresence)
        {
            // REMOVE or reentry with a different presence epoch: the old
            // manager is no longer eligible for restoration.
            it = m_retiredRuntimeByPerson.erase(it);
            continue;
        }

        // Never overwrite another active manager. Retain the retired entry
        // fail-closed if this invariant is unexpectedly violated.
        if (m_subjectRuntimeByPerson.find(it->first) !=
            m_subjectRuntimeByPerson.end())
        {
            ++it;
            continue;
        }

        const auto [restoredIt, inserted] =
            m_subjectRuntimeByPerson.try_emplace(
                it->first,
                std::move(it->second));
        (void)restoredIt;

        if (inserted)
        {
            it = m_retiredRuntimeByPerson.erase(it);
        }
        else
        {
            ++it;
        }
    }

    report.pendingRetiredTransactions =
        m_retiredRuntimeByPerson.size();
}

bool
VamGenerationScheduler::BuildInput(
    std::int64_t itsStartClockUs,
    std::uint64_t referenceTimeUs,
    const ns3::SumoPersonState& personState,
    const ns3::SumoVruSubjectSnapshot& subject,
    VamStationId stationId,
    const std::optional<ns3::SumoUtmWgs84ProjectionConfig>& sumoProjection,
    VamInput& output,
    std::string& error)
{
    if (personState.simulationTimeUs > referenceTimeUs)
    {
        return Reject(
            error,
            "SUMO person sample time is newer than the scheduler reference time");
    }

    VamInput candidate{};

    candidate.originatingStation.stationId = stationId;
    candidate.vruProfile = subject.profile;
    candidate.vruRole = subject.roleState;

    if (sumoProjection.has_value())
    {
        // SUMO x/y are projected network coordinates. Convert them only
        // when an explicit, validated network projection is available.
        ns3::SumoWgs84Position geodeticPosition{};

        if (!ns3::SumoGeodeticTransform::ToWgs84(
                *sumoProjection,
                ns3::SumoProjectedPosition{
                    personState.sample.x,
                    personState.sample.y
                },
                geodeticPosition,
                error))
        {
            return false;
        }

        // Build and validate a complete PoTi sample before publishing any
        // geodetic field into the candidate.
        VamPotiSample potiSample{};
        potiSample.latitudeDeg = geodeticPosition.latitudeDeg;
        potiSample.longitudeDeg = geodeticPosition.longitudeDeg;
        potiSample.speedMps = personState.sample.speed;
        potiSample.headingDeg = personState.sample.angleDeg;
        potiSample.positionTimeSimulationUs =
            personState.simulationTimeUs;
        potiSample.generationTimeSimulationUs = referenceTimeUs;

        // Altitude and acceleration remain unavailable: the SUMO person
        // feed does not provide validated observations for either field.

        VamPotiTimeReference potiTimeReference{};

        if (!VamPotiTemporalTransform::BuildReference(
                itsStartClockUs,
                potiSample,
                potiTimeReference,
                error))
        {
            return false;
        }

        candidate.generationTimeIts =
            std::chrono::microseconds{
                potiTimeReference.generationTimestampItsUs};

        candidate.referencePositionTimeIts =
            std::chrono::microseconds{
                potiTimeReference.positionTimestampItsUs};

        candidate.referencePosition.latitudeDeg =
            potiSample.latitudeDeg;
        candidate.referencePosition.longitudeDeg =
            potiSample.longitudeDeg;
    }
    else
    {
        // No network projection: preserve the existing temporal behavior
        // and leave latitude/longitude explicitly unavailable.
        if (!ComposeItsTime(
                itsStartClockUs,
                referenceTimeUs,
                candidate.generationTimeIts,
                error,
                "VAM generation time"))
        {
            return false;
        }

        if (!ComposeItsTime(
                itsStartClockUs,
                personState.simulationTimeUs,
                candidate.referencePositionTimeIts,
                error,
                "VAM reference position time"))
        {
            return false;
        }
    }

    candidate.highFrequencyState.speedMps =
        personState.sample.speed;

    candidate.highFrequencyState.headingDeg =
        personState.sample.angleDeg;

    // A aceleração longitudinal também permanece ausente:
    // não existe uma observação validada neste feed.

    output = std::move(candidate);
    error.clear();
    return true;
}

bool
VamGenerationScheduler::EvaluateStep(
    std::uint64_t referenceTimeUs,
    const ns3::SumoPersonRegistry& persons,
    const ns3::SumoVruSubjectRegistry& subjects,
    const ns3::VehicleRegistry& vehicles,
    const ns3::SumoVehicleLifecycleRegistry& vehicleLifecycle,
    const VamGenerationContext& context,
    std::vector<VamGenerationScheduledCandidate>& candidates,
    VamGenerationSchedulerReport& report,
    std::string& error)
{
    if (!detail::HasValidVamGenerationSchedulerInstanceId(
            m_schedulerInstanceId))
    {
        return Reject(
            error,
            "VAM scheduler instance identifier is exhausted");
    }

    if (m_config.generation.tCheckVamGenMs == 0)
    {
        return Reject(
            error,
            "VAM scheduler T_CheckVamGen must be positive");
    }

    if (m_lastStepTimeUs.has_value() &&
        referenceTimeUs < *m_lastStepTimeUs)
    {
        return Reject(
            error,
            "VAM scheduler logical SUMO time regressed");
    }

    VamGenerationSchedulerReport nextReport{};

    const auto& activePersons = persons.GetActivePersons();

    // Snapshot the authoritative SUMO presence epochs before retrying
    // retired transactions. A successful retry restores the original
    // manager only if this exact presence is still active.
    std::map<std::string, VamPersonPresenceEpoch> activePresenceByPerson;
    for (const auto& [personId, personState] : activePersons)
    {
        activePresenceByPerson.emplace(
            personId,
            VamPersonPresenceEpoch{personState.firstObservedAtUs});
    }

    // Retired aborts are independent of the staged evaluation transaction.
    // Their successful resolution must survive a later subject-evaluation
    // failure and must run before the cadence early-return below.
    RetryRetiredTransactions(activePresenceByPerson, nextReport);

    // Defensive fallback for callers that update PersonRegistry directly
    // without the controller's immediate accepted-REMOVE invalidation.
    // Never erase a pending manager merely because its person disappeared.
    std::vector<std::string> absentRuntimeIds;
    for (const auto& [personId, runtime] : m_subjectRuntimeByPerson)
    {
        (void)runtime;
        if (activePersons.find(personId) == activePersons.end())
        {
            absentRuntimeIds.push_back(personId);
        }
    }

    for (const auto& personId : absentRuntimeIds)
    {
        std::string invalidationError;
        (void)InvalidatePersonPresence(personId, invalidationError);
    }

    // Stage only active runtimes. Retired runtimes live in the independent
    // ledger and cannot be rolled back by a failed subject evaluation.
    auto stagedRuntime = m_subjectRuntimeByPerson;
    std::vector<VamGenerationScheduledCandidate> nextCandidates;
    nextReport.pendingRetiredTransactions =
        m_retiredRuntimeByPerson.size();

    const std::uint64_t checkIntervalUs =
        static_cast<std::uint64_t>(
            m_config.generation.tCheckVamGenMs) * 1000ULL;

    if (m_lastEvaluationTimeUs.has_value() &&
        referenceTimeUs - *m_lastEvaluationTimeUs <
            checkIntervalUs)
    {
        nextReport.skippedByCadence = true;

        m_subjectRuntimeByPerson = std::move(stagedRuntime);
        m_lastStepTimeUs = referenceTimeUs;

        candidates = std::move(nextCandidates);
        report = nextReport;
        error.clear();
        return true;
    }

    auto& identityAuthority =
        ns3::ItsStationIdentityAuthority::Instance();

    for (const auto& [personId, personState] : activePersons)
    {
        if (m_retiredRuntimeByPerson.find(personId) !=
            m_retiredRuntimeByPerson.end())
        {
            ++nextReport.skippedQuarantinedSubjects;
            continue;
        }

        const ns3::ItsStationIdentityOwner owner{
            ns3::ItsStationIdentityOwnerKind::ItsStation,
            "vam-vru:" + personId
        };

        const auto reservedStationId =
            identityAuthority.FindStationId(owner);

        if (!reservedStationId.has_value() ||
            !identityAuthority.IsReservedBy(
                *reservedStationId, owner))
        {
            ++nextReport.skippedMissingIdentity;

            // An externally released identity must not leave a usable
            // candidate or manager state behind.
            stagedRuntime.erase(personId);
            continue;
        }

        const auto subject = subjects.Find(personId);

        if (!subject.has_value())
        {
            ++nextReport.skippedMissingSubject;
            stagedRuntime.erase(personId);
            continue;
        }

        const VamPersonPresenceEpoch presenceEpoch{
            personState.firstObservedAtUs};
        auto runtimeIt = stagedRuntime.find(personId);

        // A personId can be reused after REMOVE. Never attach a new SUMO
        // presence to a manager created for an earlier presence epoch.
        if (runtimeIt != stagedRuntime.end() &&
            runtimeIt->second.presenceEpoch != presenceEpoch)
        {
            stagedRuntime.erase(runtimeIt);
            runtimeIt = stagedRuntime.end();
        }

        if (runtimeIt == stagedRuntime.end())
        {
            runtimeIt = stagedRuntime.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(personId),
                std::forward_as_tuple(
                    m_config.generation, presenceEpoch)).first;
        }

        auto& runtime = runtimeIt->second;

        if (runtime.pendingCandidate.has_value())
        {
            ++nextReport.skippedPendingTransactions;
            continue;
        }

        VamInput input{};

        if (!BuildInput(
                m_config.itsStartClockUs,
                referenceTimeUs,
                personState,
                *subject,
                VamStationId{*reservedStationId},
                m_config.sumoProjection,
                input,
                error))
        {
            return false;
        }

        // SUMO class does not imply role activation or animal equipment.
        // Role and VBS state remain independently controlled by the subject
        // registry; animal equipment confirmation is deliberately false.
        const auto permissions =
            vru::role::VruRolePolicy::Evaluate(
                subject->roleState,
                subject->profile,
                false);

        VamGenerationEvaluationAdapter adapter(
            runtime.generationManager,
            runtime.proximityProvider);

        VamGenerationResult decision{};

        if (!adapter.EvaluateForEgo(
                referenceTimeUs,
                personId,
                persons,
                vehicles,
                vehicleLifecycle,
                input,
                permissions,
                context,
                m_config.evaluation,
                decision,
                error))
        {
            return false;
        }

        ++nextReport.evaluatedSubjects;

        if (decision.outcome ==
            VamGenerationOutcome::BlockedByPolicy)
        {
            ++nextReport.blockedByPolicy;
        }

        if (decision.outcome ==
            VamGenerationOutcome::GenerationCandidate)
        {
            if (!decision.shouldGenerate ||
                !decision.transactionId.has_value() ||
                *decision.transactionId == 0)
            {
                return Reject(
                    error,
                    "Generation manager returned an invalid VAM candidate transaction");
            }

            VamGenerationScheduledCandidate candidate{};
            candidate.egoPersonId = personId;
            candidate.stationId = VamStationId{*reservedStationId};
            candidate.input = std::move(input);
            candidate.decision = std::move(decision);
            candidate.transactionHandle = VamGenerationTransactionHandle{
                *m_schedulerInstanceId,
                personId,
                runtime.presenceEpoch,
                *candidate.decision.transactionId};

            nextCandidates.push_back(candidate);
            runtime.pendingCandidate = std::move(candidate);
        }
    }

    // Commit staged managers, proximity histories and cadence only after
    // every eligible subject has been evaluated successfully.
    m_subjectRuntimeByPerson = std::move(stagedRuntime);
    m_lastStepTimeUs = referenceTimeUs;
    m_lastEvaluationTimeUs = referenceTimeUs;

    candidates = std::move(nextCandidates);
    report = nextReport;
    error.clear();
    return true;
}

VamGenerationScheduler::SubmittedTransactionKey
VamGenerationScheduler::MakeSubmittedTransactionKey(
    const VamGenerationTransactionHandle& handle)
{
    return SubmittedTransactionKey{
        handle.SchedulerInstanceId().value,
        handle.PersonId(),
        handle.PresenceEpoch().value,
        handle.TransactionId()
    };
}

bool
VamGenerationScheduler::MarkGenerationSubmitted(
    const VamGenerationTransactionHandle& handle,
    std::string& error)
{
    if (!m_schedulerInstanceId.has_value() ||
        handle.SchedulerInstanceId() != *m_schedulerInstanceId)
    {
        return Reject(
            error,
            "VAM transaction handle belongs to a different scheduler instance");
    }

    if (handle.TransactionId() == 0 || handle.PersonId().empty())
    {
        return Reject(error, "VAM transaction handle is invalid");
    }

    const auto runtimeIt =
        m_subjectRuntimeByPerson.find(handle.PersonId());
    if (runtimeIt == m_subjectRuntimeByPerson.end() ||
        !runtimeIt->second.pendingCandidate.has_value())
    {
        return Reject(
            error,
            "No active pending VAM candidate exists for submission");
    }

    auto& runtime = runtimeIt->second;
    const auto& candidate = *runtime.pendingCandidate;
    if (runtime.presenceEpoch != handle.PresenceEpoch() ||
        candidate.egoPersonId != handle.PersonId() ||
        !candidate.transactionHandle.has_value() ||
        *candidate.transactionHandle != handle ||
        !candidate.decision.transactionId.has_value() ||
        *candidate.decision.transactionId != handle.TransactionId())
    {
        return Reject(
            error,
            "VAM transaction handle does not match the active pending candidate");
    }

    if (runtime.submitted ||
        runtime.terminalTransportSuccess.has_value() ||
        runtime.terminalConfirmedAtMs.has_value())
    {
        return Reject(
            error,
            "VAM generation transaction has already been submitted");
    }

    // Mark before BTP: its failure paths may invoke completion synchronously.
    runtime.submitted = true;
    error.clear();
    return true;
}

bool
VamGenerationScheduler::ResolveSubmittedRuntime(
    SubjectRuntime& runtime,
    const VamGenerationTransactionHandle& handle,
    std::string& error)
{
    if (!runtime.submitted ||
        !runtime.pendingCandidate.has_value() ||
        !runtime.terminalTransportSuccess.has_value() ||
        !runtime.terminalConfirmedAtMs.has_value())
    {
        return Reject(
            error,
            "Submitted VAM transaction has no retained terminal transport result");
    }

    const auto& candidate = *runtime.pendingCandidate;
    if (runtime.presenceEpoch != handle.PresenceEpoch() ||
        candidate.egoPersonId != handle.PersonId() ||
        !candidate.transactionHandle.has_value() ||
        *candidate.transactionHandle != handle ||
        !candidate.decision.transactionId.has_value() ||
        *candidate.decision.transactionId != handle.TransactionId())
    {
        return Reject(
            error,
            "VAM terminal result does not match its retained transaction");
    }

    bool resolved = false;
    if (*runtime.terminalTransportSuccess)
    {
        resolved = runtime.generationManager.ConfirmGeneration(
            handle.TransactionId(),
            *runtime.terminalConfirmedAtMs,
            error);
    }
    else
    {
        // Failed manager aborts retain the terminal outcome for drain retry.
        resolved = m_abortExecutor(
            runtime.generationManager,
            handle.TransactionId(),
            error);
    }

    if (!resolved)
    {
        return false;
    }

    runtime.pendingCandidate.reset();
    runtime.submitted = false;
    runtime.terminalTransportSuccess.reset();
    runtime.terminalConfirmedAtMs.reset();
    error.clear();
    return true;
}

bool
VamGenerationScheduler::CompleteGeneration(
    const VamGenerationTransactionHandle& handle,
    bool success,
    std::int64_t confirmedAtMs,
    std::string& error)
{
    if (!m_schedulerInstanceId.has_value() ||
        handle.SchedulerInstanceId() != *m_schedulerInstanceId)
    {
        return Reject(
            error,
            "VAM transaction handle belongs to a different scheduler instance");
    }

    if (handle.TransactionId() == 0 || handle.PersonId().empty())
    {
        return Reject(error, "VAM transaction handle is invalid");
    }

    SubjectRuntime* runtime = nullptr;
    auto inFlightIt = m_inFlightRuntimeByHandle.find(
        MakeSubmittedTransactionKey(handle));
    const bool isRetiredInFlight =
        inFlightIt != m_inFlightRuntimeByHandle.end();

    // Prefer the full-handle ledger: an older presence may coexist with a
    // newer active presence carrying the same personId.
    if (isRetiredInFlight)
    {
        runtime = &inFlightIt->second;
    }
    else
    {
        const auto runtimeIt =
            m_subjectRuntimeByPerson.find(handle.PersonId());
        if (runtimeIt == m_subjectRuntimeByPerson.end())
        {
            return Reject(
                error,
                "No active or in-flight VAM transaction matches this handle");
        }
        runtime = &runtimeIt->second;
    }

    if (!runtime->submitted || !runtime->pendingCandidate.has_value())
    {
        return Reject(
            error,
            "VAM transaction was not submitted or is already resolved");
    }

    const auto& candidate = *runtime->pendingCandidate;
    if (runtime->presenceEpoch != handle.PresenceEpoch() ||
        candidate.egoPersonId != handle.PersonId() ||
        !candidate.transactionHandle.has_value() ||
        *candidate.transactionHandle != handle ||
        !candidate.decision.transactionId.has_value() ||
        *candidate.decision.transactionId != handle.TransactionId())
    {
        return Reject(
            error,
            "VAM completion handle does not match the submitted candidate");
    }

    // A repeated identical call can retry a failed manager resolution; a
    // conflicting terminal result is always rejected.
    if (runtime->terminalTransportSuccess.has_value())
    {
        if (*runtime->terminalTransportSuccess != success ||
            !runtime->terminalConfirmedAtMs.has_value() ||
            *runtime->terminalConfirmedAtMs != confirmedAtMs)
        {
            return Reject(
                error,
                "Conflicting terminal result for submitted VAM transaction");
        }
    }
    else
    {
        runtime->terminalTransportSuccess = success;
        runtime->terminalConfirmedAtMs = confirmedAtMs;
    }

    if (!ResolveSubmittedRuntime(*runtime, handle, error))
    {
        return false;
    }

    if (isRetiredInFlight)
    {
        m_inFlightRuntimeByHandle.erase(inFlightIt);
    }

    error.clear();
    return true;
}

bool
VamGenerationScheduler::ConfirmGeneration(
    const VamGenerationTransactionHandle& handle,
    std::int64_t confirmedAtMs,
    std::string& error)
{
    if (!m_schedulerInstanceId.has_value() ||
        handle.SchedulerInstanceId() != *m_schedulerInstanceId)
    {
        return Reject(
            error,
            "VAM transaction handle belongs to a different scheduler instance");
    }

    if (handle.TransactionId() == 0 || handle.PersonId().empty())
    {
        return Reject(error, "VAM transaction handle is invalid");
    }

    const auto runtimeIt =
        m_subjectRuntimeByPerson.find(handle.PersonId());

    if (runtimeIt == m_subjectRuntimeByPerson.end() ||
        !runtimeIt->second.pendingCandidate.has_value())
    {
        return Reject(
            error,
            "No pending VAM candidate exists for this transaction handle");
    }

    const auto& runtime = runtimeIt->second;
    const auto& candidate = *runtime.pendingCandidate;

    if (runtime.submitted)
    {
        return Reject(
            error,
            "Submitted VAM transactions must be resolved by CompleteGeneration");
    }

    if (runtime.presenceEpoch != handle.PresenceEpoch() ||
        candidate.egoPersonId != handle.PersonId() ||
        !candidate.transactionHandle.has_value() ||
        *candidate.transactionHandle != handle ||
        !candidate.decision.transactionId.has_value() ||
        *candidate.decision.transactionId != handle.TransactionId())
    {
        return Reject(
            error,
            "VAM transaction handle does not match the pending candidate");
    }

    if (!runtimeIt->second.generationManager.ConfirmGeneration(
            handle.TransactionId(),
            confirmedAtMs,
            error))
    {
        return false;
    }

    runtimeIt->second.pendingCandidate.reset();
    error.clear();
    return true;
}

bool
VamGenerationScheduler::AbortGeneration(
    const VamGenerationTransactionHandle& handle,
    std::string& error)
{
    if (!m_schedulerInstanceId.has_value() ||
        handle.SchedulerInstanceId() != *m_schedulerInstanceId)
    {
        return Reject(
            error,
            "VAM transaction handle belongs to a different scheduler instance");
    }

    if (handle.TransactionId() == 0 || handle.PersonId().empty())
    {
        return Reject(error, "VAM transaction handle is invalid");
    }

    const auto runtimeIt =
        m_subjectRuntimeByPerson.find(handle.PersonId());

    if (runtimeIt == m_subjectRuntimeByPerson.end() ||
        !runtimeIt->second.pendingCandidate.has_value())
    {
        return Reject(
            error,
            "No pending VAM candidate exists for this transaction handle");
    }

    const auto& runtime = runtimeIt->second;
    const auto& candidate = *runtime.pendingCandidate;

    if (runtime.submitted)
    {
        return Reject(
            error,
            "Submitted VAM transactions must be resolved by CompleteGeneration");
    }

    if (runtime.presenceEpoch != handle.PresenceEpoch() ||
        candidate.egoPersonId != handle.PersonId() ||
        !candidate.transactionHandle.has_value() ||
        *candidate.transactionHandle != handle ||
        !candidate.decision.transactionId.has_value() ||
        *candidate.decision.transactionId != handle.TransactionId())
    {
        return Reject(
            error,
            "VAM transaction handle does not match the pending candidate");
    }

    // Keep explicit candidate aborts on the same injectable resolution path
    // as presence invalidation, retired retries and drain. The production
    // executor still delegates to GenerationManager::AbortGeneration().
    if (!m_abortExecutor(
            runtimeIt->second.generationManager,
            handle.TransactionId(),
            error))
    {
        return false;
    }

    runtimeIt->second.pendingCandidate.reset();
    error.clear();
    return true;
}


bool
VamGenerationScheduler::DrainTransactions(std::string& error)
{
    const std::size_t awaitingTransport =
        GetInFlightTransactionCount();
    if (awaitingTransport != 0)
    {
        error =
            "VAM transaction drain blocked; transport completions pending=" +
            std::to_string(awaitingTransport);
        return false;
    }

    const bool hadTransientState =
        !m_subjectRuntimeByPerson.empty() ||
        !m_retiredRuntimeByPerson.empty() ||
        !m_inFlightRuntimeByHandle.empty();

    bool allResolved = true;
    std::string firstFailure;

    const auto recordFailure =
        [&allResolved, &firstFailure](
            const std::string& personId,
            const std::string& reason)
        {
            allResolved = false;

            if (firstFailure.empty())
            {
                firstFailure =
                    "person_id=" + personId + ": " +
                    (reason.empty()
                         ? "VAM transaction abort failed"
                         : reason);
            }
        };

    // Retry terminal outcomes whose transport callback has arrived.
    // This ledger is keyed by full handle and may coexist with a new presence.
    for (auto it = m_inFlightRuntimeByHandle.begin();
         it != m_inFlightRuntimeByHandle.end();)
    {
        auto& runtime = it->second;
        if (!runtime.pendingCandidate.has_value() ||
            !runtime.submitted ||
            !runtime.terminalTransportSuccess.has_value() ||
            !runtime.terminalConfirmedAtMs.has_value() ||
            !runtime.pendingCandidate->transactionHandle.has_value())
        {
            recordFailure(
                runtime.pendingCandidate.has_value()
                    ? runtime.pendingCandidate->egoPersonId
                    : std::string("unknown"),
                "in-flight ledger contains an invalid or unresolved transaction");
            ++it;
            continue;
        }

        const auto handle = *runtime.pendingCandidate->transactionHandle;
        std::string resolveError;
        if (ResolveSubmittedRuntime(runtime, handle, resolveError))
        {
            it = m_inFlightRuntimeByHandle.erase(it);
        }
        else
        {
            recordFailure(handle.PersonId(), resolveError);
            ++it;
        }
    }

    // Retry terminal results retained on active submitted candidates. Never
    // reinterpret them as unsent work and invoke AbortRuntimeCandidate().
    for (auto& [personId, runtime] : m_subjectRuntimeByPerson)
    {
        if (!runtime.submitted)
        {
            continue;
        }

        if (!runtime.pendingCandidate.has_value() ||
            !runtime.terminalTransportSuccess.has_value() ||
            !runtime.terminalConfirmedAtMs.has_value() ||
            !runtime.pendingCandidate->transactionHandle.has_value())
        {
            recordFailure(
                personId,
                "active submitted transaction has no terminal transport result");
            continue;
        }

        const auto handle = *runtime.pendingCandidate->transactionHandle;
        std::string resolveError;
        if (!ResolveSubmittedRuntime(runtime, handle, resolveError))
        {
            recordFailure(personId, resolveError);
        }
    }

    // First retry transactions already retained by an earlier REMOVE or
    // drain. A malformed entry remains retained fail-closed.
    for (auto it = m_retiredRuntimeByPerson.begin();
         it != m_retiredRuntimeByPerson.end();)
    {
        if (!it->second.pendingCandidate.has_value())
        {
            recordFailure(
                it->first,
                "retired transaction has no pending candidate");
            ++it;
            continue;
        }

        if (it->second.submitted)
        {
            recordFailure(
                it->first,
                "submitted transaction cannot enter the retired-abort ledger");
            ++it;
            continue;
        }

        std::string abortError;
        if (AbortRuntimeCandidate(
                it->second,
                it->first,
                false,
                abortError))
        {
            // The abort is resolved, but the scheduler may remain active
            // because another retired or active transaction can still fail.
            // Restore the original manager without its pending candidate so
            // a later evaluation cannot restart its local transaction ID.
            const std::string personId = it->first;

            if (m_subjectRuntimeByPerson.find(personId) !=
                m_subjectRuntimeByPerson.end())
            {
                recordFailure(
                    personId,
                    "cannot restore retired runtime over an active runtime");
                ++it;
                continue;
            }

            const auto [activeIt, inserted] =
                m_subjectRuntimeByPerson.try_emplace(
                    personId,
                    std::move(it->second));
            (void)activeIt;

            if (inserted)
            {
                it = m_retiredRuntimeByPerson.erase(it);
            }
            else
            {
                recordFailure(
                    personId,
                    "failed to restore resolved retired runtime");
                ++it;
            }
        }
        else
        {
            recordFailure(it->first, abortError);
            ++it;
        }
    }

    // Resolve active transactions, but retain every active manager until
    // the entire drain succeeds. A successful individual abort clears only
    // its pending candidate. Preserving the manager keeps its local
    // transaction counter monotonic if another peer's abort fails.
    for (auto it = m_subjectRuntimeByPerson.begin();
         it != m_subjectRuntimeByPerson.end();)
    {
        const std::string personId = it->first;

        if (!it->second.pendingCandidate.has_value())
        {
            ++it;
            continue;
        }

        if (it->second.submitted)
        {
            recordFailure(
                personId,
                "active submitted transaction remains unresolved");
            ++it;
            continue;
        }

        // Active and retired entries for the same person would violate the
        // one-runtime-per-presence invariant. Preserve both rather than
        // allowing an insertion collision to discard either manager.
        if (m_retiredRuntimeByPerson.find(personId) !=
            m_retiredRuntimeByPerson.end())
        {
            recordFailure(
                personId,
                "active runtime conflicts with an existing retired transaction");
            ++it;
            continue;
        }

        std::string abortError;
        if (AbortRuntimeCandidate(
                it->second,
                personId,
                true,
                abortError))
        {
            // Keep this manager until all other transactions have drained.
            ++it;
            continue;
        }

        recordFailure(personId, abortError);

        const auto [retiredIt, inserted] =
            m_retiredRuntimeByPerson.emplace(
                personId,
                std::move(it->second));
        (void)retiredIt;

        if (inserted)
        {
            it = m_subjectRuntimeByPerson.erase(it);
        }
        else
        {
            // Do not erase the active entry if insertion unexpectedly fails.
            ++it;
            recordFailure(
                personId,
                "failed to retain active transaction in retired ledger");
        }
    }

    if (!m_retiredRuntimeByPerson.empty() ||
        !m_inFlightRuntimeByHandle.empty())
    {
        allResolved = false;
    }

    // Any active candidate left here was not successfully aborted.
    for (const auto& [personId, runtime] : m_subjectRuntimeByPerson)
    {
        if (runtime.pendingCandidate.has_value())
        {
            recordFailure(
                personId,
                "active runtime still has an unresolved pending transaction");
        }
    }

    if (!allResolved)
    {
        error = firstFailure.empty()
            ? "VAM transaction drain incomplete; unresolved runtime retained"
            : "VAM transaction drain incomplete; " + firstFailure;
        return false;
    }

    // Only a globally completed drain may discard active managers whose
    // pending candidates were already resolved.
    m_subjectRuntimeByPerson.clear();

    // A completed drain ends the current transaction incarnation. Rotate
    // identity so a later evaluation cannot recreate an old opaque handle
    // from the same person, presence epoch and manager-local transaction ID.
    if (hadTransientState)
    {
        m_schedulerInstanceId = AllocateSchedulerInstanceId();
        m_lastStepTimeUs.reset();
        m_lastEvaluationTimeUs.reset();
    }

    error.clear();
    return true;
}

std::size_t
VamGenerationScheduler::GetRetiredTransactionCount() const noexcept
{
    return m_retiredRuntimeByPerson.size();
}

std::size_t
VamGenerationScheduler::GetInFlightTransactionCount() const noexcept
{
    std::size_t count = 0;
    const auto awaitingTransport = [](const SubjectRuntime& runtime)
    {
        return runtime.submitted &&
            !runtime.terminalTransportSuccess.has_value();
    };

    for (const auto& [personId, runtime] : m_subjectRuntimeByPerson)
    {
        (void)personId;
        if (awaitingTransport(runtime))
        {
            ++count;
        }
    }

    for (const auto& [key, runtime] : m_inFlightRuntimeByHandle)
    {
        (void)key;
        if (awaitingTransport(runtime))
        {
            ++count;
        }
    }

    return count;
}

bool
VamGenerationScheduler::IsPersonQuarantined(
    const std::string& personId) const noexcept
{
    return m_retiredRuntimeByPerson.find(personId) !=
        m_retiredRuntimeByPerson.end();
}

bool
VamGenerationScheduler::HasRetainedRuntimeForPresence(
    const std::string& personId,
    VamPersonPresenceEpoch presenceEpoch) const
{
    if (personId.empty())
    {
        return false;
    }

    const auto activeIt = m_subjectRuntimeByPerson.find(personId);
    if (activeIt != m_subjectRuntimeByPerson.end() &&
        activeIt->second.presenceEpoch == presenceEpoch)
    {
        return true;
    }

    const auto retiredIt = m_retiredRuntimeByPerson.find(personId);
    if (retiredIt != m_retiredRuntimeByPerson.end() &&
        retiredIt->second.presenceEpoch == presenceEpoch)
    {
        return true;
    }

    for (const auto& entry : m_inFlightRuntimeByHandle)
    {
        const auto& key = entry.first;
        if (std::get<1>(key) == personId &&
            std::get<2>(key) == presenceEpoch.value)
        {
            return true;
        }
    }

    return false;
}

bool
VamGenerationScheduler::HasValidInstanceIdentity() const noexcept
{
    return detail::HasValidVamGenerationSchedulerInstanceId(
        m_schedulerInstanceId);
}

std::vector<VamGenerationScheduledCandidate>
VamGenerationScheduler::GetPendingCandidates() const
{
    std::vector<VamGenerationScheduledCandidate> result;

    for (const auto& [personId, runtime] :
         m_subjectRuntimeByPerson)
    {
        (void)personId;

        if (runtime.pendingCandidate.has_value())
        {
            result.push_back(*runtime.pendingCandidate);
        }
    }

    return result;
}

} // namespace vam
} // namespace vanetza_integration
