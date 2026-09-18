#pragma once

#include <cstdint>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/denm/types/denm_message.h"
#include "vanetza_integration/denm/replay/denm_replay_context.h"

namespace ns3
{

class DenmReplayProtection
{
public:
    static bool Process(
        PacketContext& ctx,
        const DenmMessage& message,
        DenmReplayContext& replay);

private:
    static bool IsReplay(
        const PacketContext& ctx,
        const DenmMessage& message,
        const DenmReplayContext& replay);

    static bool Apply(
        const PacketContext& ctx,
        const DenmMessage& message,
        DenmReplayContext& replay);

    static uint64_t Fingerprint(
        const PacketContext& ctx);
};

} // namespace ns3
