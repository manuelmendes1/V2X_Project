#pragma once

#include "vanetza_integration/vam/perception/vam_proximity_snapshot_builder.h"

#include <cstdint>
#include <string>

namespace vanetza_integration
{
namespace vam
{

struct VamGenerationEvaluationAdapterConfig
{
    VamProximitySnapshotBuilderConfig snapshotBuilder{};
    VamProximityProviderConfig proximity{};
};

// Liga os snapshots SUMO à avaliação de geração.
//
// O chamador mantém a propriedade do GenerationManager e do ProximityProvider.
// O histórico de proximidade persiste entre chamadas.
// A transação devolvida por Evaluate permanece sob responsabilidade do chamador.
class VamGenerationEvaluationAdapter
{
public:
    VamGenerationEvaluationAdapter(
        VamGenerationManager& generationManager,
        VamProximityProvider& proximityProvider) noexcept;

    bool EvaluateForEgo(
        std::uint64_t referenceTimeUs,
        const std::string& egoPersonId,
        const ns3::SumoPersonRegistry& persons,
        const ns3::VehicleRegistry& vehicles,
        const ns3::SumoVehicleLifecycleRegistry& vehicleLifecycle,
        const VamInput& input,
        const vru::role::VruRolePermissions& permissions,
        const VamGenerationContext& context,
        const VamGenerationEvaluationAdapterConfig& config,
        VamGenerationResult& output,
        std::string& error);

private:
    VamGenerationManager& m_generationManager;
    VamProximityProvider& m_proximityProvider;
};

} // namespace vam
} // namespace vanetza_integration
