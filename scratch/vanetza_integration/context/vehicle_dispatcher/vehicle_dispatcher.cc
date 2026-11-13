#include "vehicle_dispatcher.h"

#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"

namespace ns3
{

void
VehicleDispatcher::ForEachReadyVehicle(
    const std::function<void(VehicleState&)>& callback)
{
    auto& registry =
        VehicleRegistry::Instance();

    const auto& lifecycle = SumoVehicleLifecycleRegistry::Instance();

    for (const auto& [vehId, statePtr] : registry.GetAllVehicles())
    {
        if (!lifecycle.IsDispatchEligible(vehId))
        {
            continue;
        }

        if (!statePtr)
        {
            continue;
        }

        VehicleState& vehicle = *statePtr;

        if (!vehicle.initialized)
        {
            continue;
        }

        if (!vehicle.socket)
        {
            continue;
        }

        if (!vehicle.node)
        {
            continue;
        }

        callback(vehicle);
    }
}

} // namespace ns3
