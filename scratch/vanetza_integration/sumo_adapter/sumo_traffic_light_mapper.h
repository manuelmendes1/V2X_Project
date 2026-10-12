#pragma once

#include "sumo_traffic_light_parser.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

#include <string>

namespace ns3 {

class SumoTrafficLightMapper
{
public:
    static bool Map(
        const SumoTrafficLightState& input,
        SpatemMovement& output,
        std::string& error);
};

} // namespace ns3
