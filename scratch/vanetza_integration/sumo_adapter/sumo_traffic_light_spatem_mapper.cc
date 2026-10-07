#include "sumo_traffic_light_spatem_mapper.h"

#include <utility>

namespace ns3
{

bool
SumoTrafficLightSpatemMapper::Map(const SumoTrafficLightState& input,
                                   const SumoSpatemTimeReference& reference,
                                   SpatemMovement& output,
                                   std::string& error)
{
    SpatemMovement movement;

    if (!SumoTrafficLightMapper::Map(input, movement, error))
    {
        return false;
    }

    if (movement.events.size() != 1)
    {
        error = "SPATEM movement mapper must produce exactly one event";
        return false;
    }

    SpatemTiming timing;

    if (!SumoTrafficLightTimingMapper::Map(input, reference, timing, error))
    {
        return false;
    }

    movement.events[0].timing = timing;
    output = std::move(movement);

    return true;
}

} // namespace ns3
