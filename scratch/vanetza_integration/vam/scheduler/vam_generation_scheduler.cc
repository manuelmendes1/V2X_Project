#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"

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
    const VamGenerationConfig& generationConfig)
    : generationManager(generationConfig)
{
}

VamGenerationScheduler::VamGenerationScheduler(
    VamGenerationSchedulerConfig config)
    : m_config(std::move(config))
{
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
VamGenerationScheduler::BuildInput(
    std::int64_t itsStartClockUs,
    std::uint64_t referenceTimeUs,
    const ns3::SumoPersonState& personState,
    const ns3::SumoVruSubjectSnapshot& subject,
    VamStationId stationId,
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

    // SUMO Person x/y são coordenadas projetadas da rede.
    // Não as converter nem reinterpretar como latitude/longitude WGS84.
    // A posição geodésica permanece explicitamente indisponível.

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

    const auto& activePersons = persons.GetActivePersons();

    // Stage all runtime state. If any subject evaluation fails, neither
    // manager state nor proximity history is committed for this step.
    auto stagedRuntime = m_subjectRuntimeByPerson;

    // SUMO REMOVE is authoritative for temporary runtime ownership.
    // Logical ITS station reservations remain in ItsStationIdentityAuthority.
    for (auto it = stagedRuntime.begin();
         it != stagedRuntime.end();)
    {
        if (activePersons.find(it->first) == activePersons.end())
        {
            it = stagedRuntime.erase(it);
        }
        else
        {
            ++it;
        }
    }

    std::vector<VamGenerationScheduledCandidate> nextCandidates;
    VamGenerationSchedulerReport nextReport{};

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

        auto runtimeIt = stagedRuntime.find(personId);

        if (runtimeIt == stagedRuntime.end())
        {
            runtimeIt = stagedRuntime.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(personId),
                std::forward_as_tuple(m_config.generation)).first;
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

bool
VamGenerationScheduler::ConfirmGeneration(
    const std::string& personId,
    std::uint64_t transactionId,
    std::int64_t confirmedAtMs,
    std::string& error)
{
    const auto runtimeIt =
        m_subjectRuntimeByPerson.find(personId);

    if (runtimeIt == m_subjectRuntimeByPerson.end() ||
        !runtimeIt->second.pendingCandidate.has_value())
    {
        return Reject(
            error,
            "No pending VAM candidate exists for this person");
    }

    const auto& candidate =
        *runtimeIt->second.pendingCandidate;

    if (!candidate.decision.transactionId.has_value() ||
        *candidate.decision.transactionId != transactionId)
    {
        return Reject(
            error,
            "VAM scheduler transaction ID mismatch");
    }

    if (!runtimeIt->second.generationManager.ConfirmGeneration(
            transactionId,
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
    const std::string& personId,
    std::uint64_t transactionId,
    std::string& error)
{
    const auto runtimeIt =
        m_subjectRuntimeByPerson.find(personId);

    if (runtimeIt == m_subjectRuntimeByPerson.end() ||
        !runtimeIt->second.pendingCandidate.has_value())
    {
        return Reject(
            error,
            "No pending VAM candidate exists for this person");
    }

    const auto& candidate =
        *runtimeIt->second.pendingCandidate;

    if (!candidate.decision.transactionId.has_value() ||
        *candidate.decision.transactionId != transactionId)
    {
        return Reject(
            error,
            "VAM scheduler transaction ID mismatch");
    }

    if (!runtimeIt->second.generationManager.AbortGeneration(
            transactionId,
            error))
    {
        return false;
    }

    runtimeIt->second.pendingCandidate.reset();
    error.clear();
    return true;
}

void
VamGenerationScheduler::ResetRuntime() noexcept
{
    m_subjectRuntimeByPerson.clear();
    m_lastStepTimeUs.reset();
    m_lastEvaluationTimeUs.reset();
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
