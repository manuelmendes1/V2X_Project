#pragma once

#include <memory>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/denm/types/denm_input.h"

namespace ns3
{

struct DenmGenerationContext
{
    std::shared_ptr<PacketContext> packetContext;
    DenmInput denmInput;
};

} // namespace ns3
