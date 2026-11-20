#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "vanetza_integration/vam/generation/vam_generation_manager.h"
#include "vanetza_integration/vam/perception/vam_generation_evaluation_adapter.h"
#include "vanetza_integration/vam/types/vam_identity_types.h"
#include "vanetza_integration/vam/types/vam_input.h"
#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"

namespace ns3
{
struct SumoPersonState;
struct SumoVruSubjectSnapshot;

class SumoPersonRegistry;
class SumoVruSubjectRegistry;
class SumoVehicleLifecycleRegistry;
class VehicleRegistry;
}

namespace vanetza_integration
{
namespace vam
{

struct VamGenerationSchedulerConfig
{
    // Relógio ITS correspondente ao início da simulação.
    std::int64_t itsStartClockUs{};

    VamGenerationConfig generation{};
    VamGenerationEvaluationAdapterConfig evaluation{};

    // Optional, explicit WGS84 projection extracted from the SUMO .net.xml.
    // Absence preserves the existing Cartesian-only evaluation behavior.
    std::optional<ns3::SumoUtmWgs84ProjectionConfig> sumoProjection;
};

using VamGenerationAbortExecutor = std::function<bool(
    VamGenerationManager& manager,
    std::uint64_t transactionId,
    std::string& error)>;

struct VamGenerationScheduledCandidate
{
    std::string egoPersonId;
    VamStationId stationId{};
    VamInput input{};
    VamGenerationResult decision{};
    std::optional<VamGenerationTransactionHandle> transactionHandle;
};

struct VamGenerationSchedulerReport
{
    std::size_t skippedMissingIdentity{};
    std::size_t skippedMissingSubject{};
    std::size_t skippedPendingTransactions{};
    std::size_t evaluatedSubjects{};
    std::size_t blockedByPolicy{};
    std::size_t retiredAbortAttempts{};
    std::size_t retiredAbortSuccesses{};
    std::size_t pendingRetiredTransactions{};
    std::size_t skippedQuarantinedSubjects{};
    bool skippedByCadence{false};
};

// Coordena a avaliação de geração VAM por pessoa SUMO.
//
// O chamador invoca EvaluateStep no final de cada passo SUMO.
// A cadência T_CheckVamGen é controlada pelo relógio lógico SUMO,
// sem timers ou callbacks próprios do ns-3.
//
// Cada pessoa possui um GenerationManager e um histórico de proximidade
// independentes. Os candidatos permanecem pendentes até confirmação
// ou aborto explícito pelo chamador.
class VamGenerationScheduler
{
public:
    explicit VamGenerationScheduler(
        VamGenerationSchedulerConfig config,
        VamGenerationAbortExecutor abortExecutor = {});

    // Reserva uma identidade ITS explicitamente fornecida pelo chamador.
    // Não deriva stationId de personId, Node ID ou MAC.
    bool RegisterSubjectStationIdentity(
        const std::string& personId,
        VamStationId stationId,
        std::string& error);

    bool EvaluateStep(
        std::uint64_t referenceTimeUs,
        const ns3::SumoPersonRegistry& persons,
        const ns3::SumoVruSubjectRegistry& subjects,
        const ns3::VehicleRegistry& vehicles,
        const ns3::SumoVehicleLifecycleRegistry& vehicleLifecycle,
        const VamGenerationContext& context,
        std::vector<VamGenerationScheduledCandidate>& candidates,
        VamGenerationSchedulerReport& report,
        std::string& error);

    bool ConfirmGeneration(
        const VamGenerationTransactionHandle& handle,
        std::int64_t confirmedAtMs,
        std::string& error);

    bool AbortGeneration(
        const VamGenerationTransactionHandle& handle,
        std::string& error);

    // Transfer an evaluated candidate to the asynchronous transport path.
    // Direct ConfirmGeneration/AbortGeneration calls are rejected afterwards.
    bool MarkGenerationSubmitted(
        const VamGenerationTransactionHandle& handle,
        std::string& error);

    // Resolve only the submitted transaction identified by the complete handle.
    bool CompleteGeneration(
        const VamGenerationTransactionHandle& handle,
        bool success,
        std::int64_t confirmedAtMs,
        std::string& error);

    // Number of submitted transactions still awaiting a transport callback.
    std::size_t GetInFlightTransactionCount() const noexcept;

    // Invalidate one accepted SUMO presence. A failed abort moves the
    // runtime and candidate to the retired ledger and quarantines personId.
    bool InvalidatePersonPresence(
        const std::string& personId,
        std::string& error);

    // Resolve every active and retired transaction before scheduler
    // replacement, disable or destruction. Failure retains unresolved
    // managers in the active or retired state for a later retry.
    bool DrainTransactions(std::string& error);

    std::size_t GetRetiredTransactionCount() const noexcept;
    bool IsPersonQuarantined(
        const std::string& personId) const noexcept;

    // True while any scheduler ledger still owns runtime state for this exact
    // SUMO presence epoch, including retired aborts and submitted transactions.
    bool HasRetainedRuntimeForPresence(
        const std::string& personId,
        VamPersonPresenceEpoch presenceEpoch) const;

    bool HasValidInstanceIdentity() const noexcept;

    std::vector<VamGenerationScheduledCandidate>
    GetPendingCandidates() const;

private:
    struct SubjectRuntime
    {
        explicit SubjectRuntime(
            const VamGenerationConfig& generationConfig,
            VamPersonPresenceEpoch presenceEpoch);

        VamPersonPresenceEpoch presenceEpoch{};
        VamGenerationManager generationManager;
        VamProximityProvider proximityProvider;

        std::optional<VamGenerationScheduledCandidate>
            pendingCandidate;

        // submitted means the transport owns this generation transaction.
        // A terminal result is retained if GenerationManager resolution fails,
        // allowing DrainTransactions() to retry without treating it as in-flight.
        bool submitted{false};
        std::optional<bool> terminalTransportSuccess;
        std::optional<std::int64_t> terminalConfirmedAtMs;
    };

    using SubmittedTransactionKey = std::tuple<
        std::uint64_t, std::string, std::uint64_t, std::uint64_t>;

    static SubmittedTransactionKey MakeSubmittedTransactionKey(
        const VamGenerationTransactionHandle& handle);

    bool ResolveSubmittedRuntime(
        SubjectRuntime& runtime,
        const VamGenerationTransactionHandle& handle,
        std::string& error);

    bool AbortRuntimeCandidate(
        SubjectRuntime& runtime,
        const std::string& personId,
        bool requireCurrentSchedulerInstance,
        std::string& error);

    void RetryRetiredTransactions(
        const std::map<std::string, VamPersonPresenceEpoch>& activePresenceByPerson,
        VamGenerationSchedulerReport& report);

    static bool BuildInput(
        std::int64_t itsStartClockUs,
        std::uint64_t referenceTimeUs,
        const ns3::SumoPersonState& personState,
        const ns3::SumoVruSubjectSnapshot& subject,
        VamStationId stationId,
        const std::optional<ns3::SumoUtmWgs84ProjectionConfig>& sumoProjection,
        VamInput& output,
        std::string& error);

    VamGenerationSchedulerConfig m_config;
    std::optional<VamGenerationSchedulerInstanceId> m_schedulerInstanceId;

    std::map<std::string, SubjectRuntime> m_subjectRuntimeByPerson;

    // Failed REMOVE aborts retain evaluated (not submitted) candidates here.
    // Presence quarantine is represented by membership in this ledger.
    std::map<std::string, SubjectRuntime> m_retiredRuntimeByPerson;

    // Removed submitted runtimes are keyed by their complete transaction identity.
    // Multiple presence epochs for one personId may coexist until completion.
    std::map<SubmittedTransactionKey, SubjectRuntime>
        m_inFlightRuntimeByHandle;

    // Empty injection selects the production GenerationManager abort.
    VamGenerationAbortExecutor m_abortExecutor;

    // Último passo SUMO aceite, incluindo passos ignorados pela cadência.
    std::optional<std::uint64_t> m_lastStepTimeUs;

    // Último passo em que a avaliação de geração foi executada.
    std::optional<std::uint64_t> m_lastEvaluationTimeUs;
};

} // namespace vam
} // namespace vanetza_integration
