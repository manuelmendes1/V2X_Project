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

    // Absolute ITS timestamp corresponding to this SUMO simulation instant.
    // Negative means that no absolute reference was constructed.
    std::int64_t absoluteItsTimestampUs = -1;
};

class SumoTrafficLightTimingMapper
{
public:
    static bool BuildReference(
        const std::string& itsStartTime,
        int64_t elapsedMicroseconds,
        SumoSpatemTimeReference& output,
        std::string& error);

    static bool Map(
        const SumoTrafficLightState& input,
        const SumoSpatemTimeReference& reference,
        SpatemTiming& output,
        std::string& error);
};

} // namespace ns3
