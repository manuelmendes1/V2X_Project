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

struct VamClusterOperation
{
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
