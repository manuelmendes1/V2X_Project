#pragma once

#include <cstdint>
#include <vector>

#include <ns3/packet.h>
#include <ns3/socket.h>


#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/rx/rx_unpack/rx_unpack.h"
#include "vanetza_integration/context/vehicle_state.h"

struct CamMessage;

namespace ns3 {
    class Packet;
    class Socket;
    struct VehicleState;
}

class RxPipeline {
    public:
        static void ProcessDirect(
            ns3::PacketContext& ctx,
            const std::vector<uint8_t>& buffer);

    private:

        static bool ExtractRaw(ns3::PacketContext& ctx, const std::vector<uint8_t>& buffer, CamPayloadParts& frame);
        static bool ResolveEndpoints(ns3::PacketContext& ctx,ns3::VehicleState*& sender,ns3::VehicleState*& receiver);
        static bool Decode(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, CamMessage& message);
        static bool Validate(ns3::PacketContext& ctx, ns3::VehicleState& sender);
        static bool Verify(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, const std::vector<uint8_t>& signature, ns3::VehicleState& sender, ns3::VehicleState& receiver);
        static bool Replay(ns3::PacketContext& ctx, const CamMessage& message, ns3::VehicleState& receiver);
        static bool Reception(ns3::PacketContext& ctx, const CamMessage& message, ns3::VehicleState& sender, ns3::VehicleState& receiver);
        static bool Deliver(ns3::PacketContext& ctx, ns3::VehicleState& sender);
};
