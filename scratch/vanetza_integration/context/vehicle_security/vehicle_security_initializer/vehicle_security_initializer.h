#pragma once

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

class VehicleSecurityInitializer
{
public:

    static void Initialize(VehicleState& state);
};

} // namespace ns3
