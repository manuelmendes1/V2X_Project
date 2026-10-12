#pragma once

#include "sumo_traffic_light_parser.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

#include <cstdint>
#include <string>

namespace ns3
{

struct SumoSpatemTimeReference
{
    uint32_t minuteOfYear = 0;
    uint16_t dSecond = 0;
};

class SumoTrafficLightTimingMapper
{
public:
    static bool Map(
        const SumoTrafficLightState& input,
        const SumoSpatemTimeReference& reference,
        SpatemTiming& output,
        std::string& error);
};

} // namespace ns3
