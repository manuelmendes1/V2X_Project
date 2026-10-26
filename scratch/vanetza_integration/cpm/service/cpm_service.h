#pragma once

#include <cstdint>
#include <memory>

#include "vanetza_integration/cpm/transport/cpm_transport_gateway.h"
#include "vanetza_integration/cpm/types/cpm_input.h"
#include "vanetza_integration/context/packet/packet_context.h"

namespace ns3
{

class CpmService
{
public:
    static bool Send(
        const std::shared_ptr<PacketContext>& context,
        const CpmInput& input,
        const CpmTransportGateway& gateway,
        uint8_t trafficClass);
};

} // namespace ns3
