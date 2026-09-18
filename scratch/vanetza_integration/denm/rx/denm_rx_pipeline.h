#pragma once

#include <cstdint>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/denm/types/denm_message.h"

namespace ns3
{

class DenmRxPipeline
{
public:
    static void Process(
        PacketContext& ctx,
        const std::vector<uint8_t>& payload);
};

} // namespace ns3
