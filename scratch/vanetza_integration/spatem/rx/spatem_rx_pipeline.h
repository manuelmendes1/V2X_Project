#pragma once

#include <cstdint>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/types/spatem_message.h"
#include "vanetza_integration/spatem/replay/spatem_replay_context.h"

namespace ns3
{

class SpatemRxPipeline
{
  public:
    static bool Process(PacketContext& context,
                        const std::vector<uint8_t>& payload,
                        SpatemMessage& message,
                        SpatemReplayContext& replay);
};

} // namespace ns3
