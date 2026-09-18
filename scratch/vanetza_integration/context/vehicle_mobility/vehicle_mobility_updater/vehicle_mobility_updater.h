#pragma once

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

class VehicleMobilityUpdater
{
public:

    static void Apply(VehicleState& state);
};

}
