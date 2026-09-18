#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vanetza_integration/cam/generation_context/cam_generation_context.h"

class CamService {
    public:
        static std::vector<uint8_t> GenerateRawPayload(ns3::CamGenerationContext& context, const std::string& reason);
};
