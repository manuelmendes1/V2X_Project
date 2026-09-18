#include "vehicle_dispatcher.h"

#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

namespace ns3
{

void
VehicleDispatcher::ForEachReadyVehicle(
    const std::function<void(VehicleState&)>& callback)
{
    auto& registry =
        VehicleRegistry::Instance();

    for (const auto& [vehId, statePtr] : registry.GetAllVehicles())
    {
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
