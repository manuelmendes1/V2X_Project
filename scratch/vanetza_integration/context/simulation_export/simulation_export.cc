#include "simulation_export.h"

#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

namespace ns3
{

void SimulationExport::SendVehiclesToSumo()
{
    const auto& registry =
        VehicleRegistry::Instance();

    for (const auto& [vehId, state] : registry.GetAllVehicles())
    {
        if (state && state->node)
        {
            SumoAdapter::SendToSumo(*state);
        }
    }
}

} // namespace ns3
