#pragma once

#include <string>

#include "sumo_traffic_light_parser.h"
#include "sumo_traffic_light_timing_mapper.h"
#include "sumo_traffic_light_mapper.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

namespace ns3
{

class SumoTrafficLightSpatemMapper
{
public:
    static bool Map(const SumoTrafficLightState& input,
                    const SumoSpatemTimeReference& reference,
                    SpatemMovement& output,
                    std::string& error);
};

} // namespace ns3
