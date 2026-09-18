#pragma once

#include "vanetza_integration/context/packet/packet_context.h"

#include <vector>

struct CamPayloadParts
{
    std::vector<uint8_t> payload;
    std::vector<uint8_t> signature;
};

class RxUnpack
{
public:
    static bool ExtractRaw(
        ns3::PacketContext& ctx,
        const std::vector<uint8_t>& buffer,
        CamPayloadParts& frame);
};
