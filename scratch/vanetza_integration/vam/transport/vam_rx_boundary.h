#pragma once

#include <cstdint>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"

namespace ns3
{

class VamRxBoundary
{
  public:
    // Completes the common GeoNetworking/BTP RX stages for a VAM
    // indication. Application security, decoding, replay and delivery
    // are deliberately outside this boundary.
    static bool Process(
        PacketContext& context,
        const std::vector<uint8_t>& payload);
};

} // namespace ns3
