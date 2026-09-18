#pragma once

#include "vanetza_integration/cam/decoder/cam_decoder.h"
#include "vanetza_integration/context/packet/packet_context.h"

#include <vector>

class RxDecode {
    public:
        static bool Process(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, CamMessage& message);
};
