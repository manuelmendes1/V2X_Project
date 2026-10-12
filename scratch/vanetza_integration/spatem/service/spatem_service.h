#pragma once

#include <memory>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/transport/spatem_transport_gateway.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

namespace ns3
{

class SpatemService
{
  public:
    static bool Send(
        const std::shared_ptr<PacketContext>& context,
        const SpatemInput& input,
        const SpatemTransportGateway& gateway,
        const std::string& reason = {});
};

} // namespace ns3
