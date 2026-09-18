#pragma once

#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_state.h"

#include <memory>

namespace ns3 {

    class CamSender {

        public:

            static void SendRaw(const std::shared_ptr<PacketContext>& ctx, VehicleState& vehicle, const std::vector<uint8_t>& rawPayload, const std::string& reason);
    };

} // namespace ns3
