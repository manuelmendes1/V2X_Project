#pragma once

#include <optional>

#include "vanetza_integration/cam/generation_context/cam_generation_context.h"
#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

class CamBuilder
{
public:

    static std::optional<CamGenerationContext> Build(
        const VehicleState& vehicle);

};

} // namespace ns3
