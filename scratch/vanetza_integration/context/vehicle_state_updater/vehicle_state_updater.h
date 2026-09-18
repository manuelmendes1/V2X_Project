#pragma once

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3{

class VehicleStateUpdater{

public:

    static void Apply(VehicleState& state, const VehicleState& source);
};

}
