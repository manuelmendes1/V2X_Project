#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "vanetza_integration/vam/generation/vam_generation_manager.h"
#include "vanetza_integration/vam/perception/vam_generation_evaluation_adapter.h"
#include "vanetza_integration/vam/types/vam_identity_types.h"
#include "vanetza_integration/vam/types/vam_input.h"

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
};

struct VamGenerationScheduledCandidate
{
    std::string egoPersonId;
    VamStationId stationId{};
    VamInput input{};
    VamGenerationResult decision{};
};

struct VamGenerationSchedulerReport
{
    std::size_t skippedMissingIdentity{};
    std::size_t skippedMissingSubject{};
    std::size_t skippedPendingTransactions{};
    std::size_t evaluatedSubjects{};
    std::size_t blockedByPolicy{};
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
        VamGenerationSchedulerConfig config);

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
        const std::string& personId,
        std::uint64_t transactionId,
        std::int64_t confirmedAtMs,
        std::string& error);

    bool AbortGeneration(
        const std::string& personId,
        std::uint64_t transactionId,
        std::string& error);

    std::vector<VamGenerationScheduledCandidate>
    GetPendingCandidates() const;

    // Limpa apenas o estado transitório desta execução.
    // As reservas de identidade ITS permanecem na autoridade global.
    void ResetRuntime() noexcept;

private:
    struct SubjectRuntime
    {
        explicit SubjectRuntime(
            const VamGenerationConfig& generationConfig);

        VamGenerationManager generationManager;
        VamProximityProvider proximityProvider;

        std::optional<VamGenerationScheduledCandidate>
            pendingCandidate;
    };

    static bool BuildInput(
        std::int64_t itsStartClockUs,
        std::uint64_t referenceTimeUs,
        const ns3::SumoPersonState& personState,
        const ns3::SumoVruSubjectSnapshot& subject,
        VamStationId stationId,
        VamInput& output,
        std::string& error);

    VamGenerationSchedulerConfig m_config;

    std::map<std::string, SubjectRuntime> m_subjectRuntimeByPerson;

    // Último passo SUMO aceite, incluindo passos ignorados pela cadência.
    std::optional<std::uint64_t> m_lastStepTimeUs;

    // Último passo em que a avaliação de geração foi executada.
    std::optional<std::uint64_t> m_lastEvaluationTimeUs;
};

} // namespace vam
} // namespace vanetza_integration
