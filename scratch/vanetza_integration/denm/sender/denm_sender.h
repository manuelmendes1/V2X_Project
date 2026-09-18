#pragma once

#include <memory>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/types/denm_input.h"

namespace ns3
{

class DenmSender
{
public:
    static bool SendRaw(
        const std::shared_ptr<PacketContext>& context,
        VehicleState& vehicle,
        const DenmInput& input,
        const std::vector<uint8_t>& rawPayload,
        double disseminationRadiusMeters,
        const std::string& reason);
};

} // namespace ns3
