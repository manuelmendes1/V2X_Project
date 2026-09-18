#pragma once

#include <functional>

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

class VehicleDispatcher
{
public:

    static void ForEachReadyVehicle(
        const std::function<void(VehicleState&)>& callback);
};

} // namespace ns3
