#pragma once

#include <string>
#include <vector>

#include "vanetza_integration/denm/generation_context/denm_generation_context.h"

namespace ns3
{

class DenmService
{
public:
    static std::vector<uint8_t> GenerateRawPayload(
        DenmGenerationContext& context,
        const std::string& reason);
};

} // namespace ns3
