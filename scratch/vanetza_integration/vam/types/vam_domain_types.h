#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "vanetza_integration/vam/types/vam_identity_types.h"
#include "vanetza_integration/vru/role/types/vru_role_types.h"

namespace vanetza_integration
{
namespace vam
{

using vanetza_integration::vru::role::VruProfile;
using vanetza_integration::vru::role::VruRoleSnapshot;

// Identidade ITS da estação emissora.
// A atribuição e unicidade de stationId pertencem à futura
// ITS Station Identity Authority, não a este modelo.
struct VamOriginatingStation
{
    VamStationId stationId{};

    // Etiqueta semântica, não um código ASN.1.
    // O mapeamento normativo fica pendente da decisão da Etapa 11.1.
    std::optional<std::string> originatingItsSTypeLabel;
};

// Posição de referência. Os campos opcionais preservam
// explicitamente a indisponibilidade dos dados.
struct VamReferencePosition
{
    std::optional<double> latitudeDeg;
    std::optional<double> longitudeDeg;
    std::optional<double> altitudeMeters;
};

// Estado cinemático de alta frequência.
struct VamHighFrequencyState
{
    std::optional<double> headingDeg;
    std::optional<double> speedMps;
    std::optional<double> longitudinalAccelerationMps2;
};

// Atributos de baixa frequência condicionais.
// O subperfil é mantido como etiqueta semântica até à
// seleção normativa e definição do respetivo mapeamento.
struct VamLowFrequencyAttributes
{
    std::optional<std::string> subProfileLabel;
};

// Extensões opcionais. A presença pode ser representada
// sem antecipar campos ASN.1 ou regras de clustering ainda
// não aprovados nas etapas normativas e de cluster.
struct VamClusterInformation
{
};

// Identificador de cluster CDD Identifier1B: 0..255.
// O valor zero é válido e não deve ser usado como sentinela de ausência.
struct VamClusterId
{
    std::uint8_t value{};

    friend bool operator==(const VamClusterId& lhs,
                           const VamClusterId& rhs)
    {
        return lhs.value == rhs.value;
    }
};

// Motivos definidos no CDD R2 ClusterLeaveReason.
enum class VamClusterLeaveReason : std::uint8_t
{
    NotProvided = 0,
    ClusterLeaderLost = 1,
    ClusterDisbandedByLeader = 2,
    OutOfClusterBoundingBox = 3,
    OutOfClusterSpeedRange = 4,
    JoiningAnotherCluster = 5,
    CancelledJoin = 6,
    FailedJoin = 7,
    SafetyCondition = 8
};

// Motivos definidos no CDD R2 ClusterBreakupReason.
enum class VamClusterBreakupReason : std::uint8_t
{
    NotProvided = 0,
    ClusteringPurposeCompleted = 1,
    LeaderMovedOutOfClusterBoundingBox = 2,
    JoiningAnotherCluster = 3,
    EnteringLowRiskAreaBasedOnMaps = 4,
    ReceptionOfCpmContainingCluster = 5
};

// Representa DeltaTimeQuarterSecond.
// duration == nullopt representa o valor ASN.1 unavailable (255).
// Valores disponíveis são múltiplos de 250 ms entre 250 e 63500 ms.
// A validação destes limites é efetuada na fronteira de validação VAM.
struct VamClusterOperationTime
{
    std::optional<std::chrono::milliseconds> duration;

    friend bool operator==(const VamClusterOperationTime& lhs,
                           const VamClusterOperationTime& rhs)
    {
        return lhs.duration == rhs.duration;
    }
};

struct VamClusterJoinInfo
{
    VamClusterId clusterId{};
    VamClusterOperationTime joinTime{};

    friend bool operator==(const VamClusterJoinInfo& lhs,
                           const VamClusterJoinInfo& rhs)
    {
        return lhs.clusterId == rhs.clusterId &&
               lhs.joinTime == rhs.joinTime;
    }
};

struct VamClusterLeaveInfo
{
    VamClusterId clusterId{};
    VamClusterLeaveReason clusterLeaveReason{
        VamClusterLeaveReason::NotProvided};

    friend bool operator==(const VamClusterLeaveInfo& lhs,
                           const VamClusterLeaveInfo& rhs)
    {
        return lhs.clusterId == rhs.clusterId &&
               lhs.clusterLeaveReason == rhs.clusterLeaveReason;
    }
};

struct VamClusterBreakupInfo
{
    VamClusterBreakupReason clusterBreakupReason{
        VamClusterBreakupReason::NotProvided};
    VamClusterOperationTime breakupTime{};

    friend bool operator==(const VamClusterBreakupInfo& lhs,
                           const VamClusterBreakupInfo& rhs)
    {
        return lhs.clusterBreakupReason == rhs.clusterBreakupReason &&
               lhs.breakupTime == rhs.breakupTime;
    }
};

// Campos independentes: o contentor pode transportar mais do que
// uma operação no mesmo VAM. Pelo menos um campo deve estar presente.
struct VamClusterOperation
{
    std::optional<VamClusterJoinInfo> clusterJoinInfo;
    std::optional<VamClusterLeaveInfo> clusterLeaveInfo;
    std::optional<VamClusterBreakupInfo> clusterBreakupInfo;

    // Optional exterior: campo ausente.
    // VamClusterOperationTime::duration ausente: valor unavailable.
    std::optional<VamClusterOperationTime> clusterIdChangeTimeInfo;

    friend bool operator==(const VamClusterOperation& lhs,
                           const VamClusterOperation& rhs)
    {
        return lhs.clusterJoinInfo == rhs.clusterJoinInfo &&
               lhs.clusterLeaveInfo == rhs.clusterLeaveInfo &&
               lhs.clusterBreakupInfo == rhs.clusterBreakupInfo &&
               lhs.clusterIdChangeTimeInfo == rhs.clusterIdChangeTimeInfo;
    }
};

struct VamMotionPrediction
{
};

struct VamSemanticData
{
    VamOriginatingStation originatingStation{};

    VruProfile vruProfile{VruProfile::Unknown};
    VruRoleSnapshot vruRole{};

    // Tempo ITS em microssegundos, coerente com a fronteira
    // temporal PoTi atualmente definida para VAM.
    std::chrono::microseconds generationTimeIts{};
    std::chrono::microseconds referencePositionTimeIts{};

    VamReferencePosition referencePosition{};
    VamHighFrequencyState highFrequencyState{};

    std::optional<VamLowFrequencyAttributes> lowFrequencyAttributes;
    std::optional<VamClusterInformation> clusterInformation;
    std::optional<VamClusterOperation> clusterOperation;
    std::optional<VamMotionPrediction> motionPrediction;
};

} // namespace vam
} // namespace vanetza_integration
