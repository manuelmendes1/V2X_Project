#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "vanetza_integration/vam/types/vam_input.h"
#include "vanetza_integration/vru/role/types/vru_role_types.h"

namespace vanetza_integration
{
namespace vam
{

enum class VamGenerationOutcome
{
    NotDue,
    NoTrigger,
    BlockedByPolicy,
    SuppressedByRedundancy,
    GenerationCandidate
};

enum class VamGenerationReason
{
    InitialActivation,
    PositionChanged,
    GroundSpeedChanged,
    GroundVelocityOrientationChanged,
    MaximumGenerationInterval,
    TrajectoryInterceptionProbabilityChanged,
    ClusterOperationChanged
};

struct VamPeerObservation
{
    VamReferencePosition referencePosition{};
    VamHighFrequencyState highFrequencyState{};
};

struct VamRedundancyContext
{
    // Uma observação de par só é usada enquanto o último VAM ego estiver
    // dentro da janela numSkipVamsForRedundancyMitigation * T_GenVamMax.
    std::optional<VamPeerObservation> similarPeer;

    // Sinais alternativos de mitigação. Cada um é suficiente isoladamente.
    bool inProtectedOrNonDrivableArea{false};
    bool inPedestrianOnlyLowRiskArea{false};
    // Condição alternativa de supressão prevista na cláusula 6.4.3 da
    // ETSI TS 103 300-3 V2.3.1; o sinal é fornecido pela política/LDM.
    bool egoReportedByAnotherItsSWithinTGenVam{false};
};

struct VamGenerationContext
{
    // Intervalo fornecido pela gestão/DCC. Ausente significa T_GenVamMin.
    std::optional<std::uint32_t> dccTGenVamMs;
    // Probabilidade validada por um modelo de trajetória externo, em [0,100].
    std::optional<double> trajectoryInterceptionProbabilityPercent;
    VamRedundancyContext redundancy{};
};

struct VamGenerationConfig
{
    std::uint32_t tGenVamMinMs{100};
    std::uint32_t tGenVamMaxMs{5000};
    std::uint32_t tGenVamLfMinMs{2000};
    // O scheduler externo deve chamar Evaluate a cada T_CheckVamGen.
    // Esta classe valida o valor, mas não cria timers nem agenda callbacks.
    std::uint32_t tCheckVamGenMs{100};

    double minPositionChangeThresholdMeters{4.0};
    double minGroundSpeedChangeThresholdMps{0.5};
    double minGroundVelocityOrientationChangeThresholdDegrees{4.0};
    double trajectoryInterceptionProbabilityChangeThresholdPercentagePoints{10.0};

    std::uint8_t numSkipVamsForRedundancyMitigation{4};
};

struct VamGenerationResult
{
    VamGenerationOutcome outcome{VamGenerationOutcome::NotDue};
    bool shouldGenerate{false};
    VamGenerationReason reason{VamGenerationReason::InitialActivation};
    bool includeLowFrequencyContainer{false};
    std::uint32_t effectiveTGenVamMs{100};
    std::optional<std::uint64_t> transactionId;
};

class VamGenerationManager
{
public:
    explicit VamGenerationManager(VamGenerationConfig config = {});

    // Avalia o estado usando o relógio lógico de simulação em milissegundos.
    // Uma decisão de geração abre uma transação; só ConfirmGeneration confirma
    // os timestamps e o estado semântico que servirão de baseline futura.
    // Na integração, confirmar apenas após o VAM ter sido entregue com sucesso
    // à camada Networking & Transport; abortar se a construção/entrega falhar.
    bool Evaluate(
        std::int64_t nowMs,
        const VamInput& input,
        const vru::role::VruRolePermissions& permissions,
        const VamGenerationContext& context,
        VamGenerationResult& output,
        std::string& error);

    bool ConfirmGeneration(std::uint64_t transactionId, std::string& error);
    bool AbortGeneration(std::uint64_t transactionId, std::string& error);

private:
    struct SemanticSnapshot
    {
        VamReferencePosition referencePosition{};
        VamHighFrequencyState highFrequencyState{};
        std::optional<double> trajectoryInterceptionProbabilityPercent;
        bool hasClusterOperation{false};
    };

    struct PendingGeneration
    {
        std::uint64_t transactionId{};
        std::int64_t evaluatedAtMs{};
        bool includeLowFrequencyContainer{false};
        SemanticSnapshot snapshot{};
    };

    VamGenerationConfig m_config;
    std::optional<std::int64_t> m_lastEvaluationAtMs;
    std::optional<std::int64_t> m_lastGenerationAtMs;
    std::optional<std::int64_t> m_lastLowFrequencyContainerAtMs;
    std::optional<SemanticSnapshot> m_lastConfirmedSnapshot;
    std::optional<PendingGeneration> m_pendingGeneration;
    std::uint64_t m_nextTransactionId{1};
    std::uint8_t m_consecutiveRedundancySkips{0};
    bool m_lastEvaluationWasEligible{false};
};

} // namespace vam
} // namespace vanetza_integration
