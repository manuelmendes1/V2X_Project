#pragma once

#include "vanetza_integration/cam/decoder/cam_decoder.h"
#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_state.h"

namespace ns3{

class CamReception{

public:

    static void Process(PacketContext& ctx, const CamMessage& message, VehicleState& receiver, const VehicleState& sender);
};

}
