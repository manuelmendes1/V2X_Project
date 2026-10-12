#pragma once

#include <cstdint>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/replay/spatem_replay_context.h"
#include "vanetza_integration/spatem/types/spatem_message.h"

namespace ns3
{

class SpatemReplayProtection
{
  public:
    static bool Process(
        PacketContext& context,
        const SpatemMessage& message,
        SpatemReplayContext& replay);

  private:
    static uint64_t Fingerprint(
        const PacketContext& context);

    static uint64_t SemanticTimestamp(
        const SpatemIntersection& intersection);

    static bool HasSemanticTimestamp(
        const SpatemIntersection& intersection);
};

} // namespace ns3
