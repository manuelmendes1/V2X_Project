#pragma once

#include <cstdint>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/cpm/replay/cpm_replay_context.h"
#include "vanetza_integration/cpm/reception/cpm_reception_queue.h"
#include "vanetza_integration/cpm/types/cpm_message.h"

namespace ns3
{

class CpmRxPipeline
{
public:
    static bool Process(
        PacketContext& context,
        const std::vector<std::uint8_t>& payload,
        CpmMessage& message,
        std::int64_t receivedAtUnixMs,
        CpmReplayContext& replay,
        CpmReceptionQueue& receptionQueue);
};

} // namespace ns3
