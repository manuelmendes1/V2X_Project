#pragma once

#include "vanetza_integration/vam/perception/vam_proximity_provider.h"

#include <string>
#include <vector>

namespace ns3
{
class SumoPersonRegistry;
class SumoVehicleLifecycleRegistry;
class VehicleRegistry;
}

namespace vanetza_integration
{
namespace vam
{

struct VamProximitySnapshotBuilderConfig
{
    // Valores fornecidos explicitamente pela política de perceção.
    // Não são defaults normativos.
    VamProximityThresholds vehicleThresholds{};
    VamProximityThresholds vruThresholds{};
};

struct VamProximitySnapshotSet
{
    VamProximityEgoSnapshot ego{};
    std::vector<VamProximityTargetSnapshot> targets;
};

class VamProximitySnapshotBuilder
{
public:
    // Constrói snapshots exclusivamente a partir dos registos SUMO
    // autoritativos. Não altera registos nem mantém histórico próprio.
    static bool BuildForEgo(
        const std::string& egoPersonId,
        const ns3::SumoPersonRegistry& persons,
        const ns3::VehicleRegistry& vehicles,
        const ns3::SumoVehicleLifecycleRegistry& vehicleLifecycle,
        const VamProximitySnapshotBuilderConfig& config,
        VamProximitySnapshotSet& output,
        std::string& error);
};

} // namespace vam
} // namespace vanetza_integration
