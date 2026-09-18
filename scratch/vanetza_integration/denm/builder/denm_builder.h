#pragma once

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/generation_context/denm_generation_context.h"

namespace ns3
{

class DenmBuilder
{
public:
    static DenmGenerationContext Build(
        VehicleState& vehicle);
};

} // namespace ns3
