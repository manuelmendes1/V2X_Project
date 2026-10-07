#include "sumo_traffic_light_mapper.h"

namespace ns3 {

bool
SumoTrafficLightMapper::Map(
    const SumoTrafficLightState& input,
    SpatemMovement& output,
    std::string& error)
{
    SpatemMovementPhaseState phaseState;

    switch (input.rawState) {
    case 'G':
        phaseState =
            SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;
        break;

    case 'g':
        phaseState =
            SpatemMovementPhaseState::PERMISSIVE_MOVEMENT_ALLOWED;
        break;

    case 'Y':
        phaseState =
            SpatemMovementPhaseState::PROTECTED_CLEARANCE;
        break;

    case 'y':
        phaseState =
            SpatemMovementPhaseState::PERMISSIVE_CLEARANCE;
        break;

    case 'R':
    case 'r':
        phaseState =
            SpatemMovementPhaseState::STOP_AND_REMAIN;
        break;

    default:
        error =
            "unsupported SUMO traffic-light state: " +
            std::string(1, input.rawState);
        return false;
    }

    output.signalGroup =
        static_cast<uint8_t>(input.linkIndex);

    output.events.clear();

    SpatemMovementEvent event;
    event.phaseState = phaseState;

    output.events.push_back(event);

    error.clear();
    return true;
}

} // namespace ns3
