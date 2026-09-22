#pragma once

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/generation_context/denm_generation_context.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"

namespace ns3
{

class DenmBuilder
{
public:
    static DenmGenerationContext Build(
        VehicleState& vehicle,
        const DenmTriggerRequest& request,
        const DenmLifecycleResult& lifecycleResult);
};

} // namespace ns3
