#include "vanetza_integration/sumo_adapter/sumo_traffic_light_mapper.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_parser.h"

#include "vanetza_integration/spatem/types/spatem_input.h"

#include <iostream>
#include <string>

namespace {

bool
ExpectMapped(
    uint32_t linkIndex,
    char rawState,
    SpatemMovementPhaseState expected)
{
    ns3::SumoTrafficLightState sumoState;

    sumoState.trafficLightId = "B0";
    sumoState.linkIndex = linkIndex;
    sumoState.rawState = rawState;
    sumoState.phaseIndex = 0;
    sumoState.phaseDuration = 30.0;
    sumoState.nextSwitch = 30.0;
    sumoState.programId = 1;
    sumoState.simulationTime = 0.0;

    SpatemMovement movement;
    std::string error;

    if (!ns3::SumoTrafficLightMapper::Map(
            sumoState,
            movement,
            error)) {
        std::cerr
            << "FAIL mapping rejected supported state "
            << rawState
            << " linkIndex=" << linkIndex
            << ": " << error << '\n';
        return false;
    }

    if (movement.signalGroup != linkIndex) {
        std::cerr
            << "FAIL signalGroup does not preserve linkIndex: "
            << linkIndex << '\n';
        return false;
    }

    if (movement.events.size() != 1) {
        std::cerr
            << "FAIL expected exactly one movement event\n";
        return false;
    }

    if (movement.events.front().phaseState != expected) {
        std::cerr
            << "FAIL unexpected SPATEM phaseState for "
            << rawState << '\n';
        return false;
    }

    return true;
}

bool
ExpectRejected(char rawState)
{
    ns3::SumoTrafficLightState sumoState;

    sumoState.trafficLightId = "B0";
    sumoState.linkIndex = 0;
    sumoState.rawState = rawState;

    SpatemMovement movement;
    std::string error;

    if (ns3::SumoTrafficLightMapper::Map(
            sumoState,
            movement,
            error)) {
        std::cerr
            << "FAIL unsupported SUMO state accepted: "
            << rawState << '\n';
        return false;
    }

    return true;
}

} // namespace

int
main()
{
    if (!ExpectMapped(
            0,
            'G',
            SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED)) {
        return 1;
    }

    if (!ExpectMapped(
            1,
            'G',
            SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED)) {
        return 1;
    }

    if (!ExpectMapped(
            0,
            'g',
            SpatemMovementPhaseState::PERMISSIVE_MOVEMENT_ALLOWED)) {
        return 1;
    }

    if (!ExpectMapped(
            1,
            'g',
            SpatemMovementPhaseState::PERMISSIVE_MOVEMENT_ALLOWED)) {
        return 1;
    }

    if (!ExpectMapped(
            0,
            'Y',
            SpatemMovementPhaseState::PROTECTED_CLEARANCE)) {
        return 1;
    }

    if (!ExpectMapped(
            1,
            'Y',
            SpatemMovementPhaseState::PROTECTED_CLEARANCE)) {
        return 1;
    }

    if (!ExpectMapped(
            0,
            'y',
            SpatemMovementPhaseState::PERMISSIVE_CLEARANCE)) {
        return 1;
    }

    if (!ExpectMapped(
            1,
            'y',
            SpatemMovementPhaseState::PERMISSIVE_CLEARANCE)) {
        return 1;
    }

    if (!ExpectMapped(
            0,
            'R',
            SpatemMovementPhaseState::STOP_AND_REMAIN)) {
        return 1;
    }

    if (!ExpectMapped(
            1,
            'r',
            SpatemMovementPhaseState::STOP_AND_REMAIN)) {
        return 1;
    }

    if (!ExpectRejected('s')) {
        return 1;
    }

    if (!ExpectRejected('u')) {
        return 1;
    }

    if (!ExpectRejected('o')) {
        return 1;
    }

    if (!ExpectRejected('O')) {
        return 1;
    }

    std::cout
        << "PASS linkIndex 0 -> signalGroup 0\n"
        << "PASS linkIndex 1 -> signalGroup 1\n"
        << "PASS G/g/Y/y/R/r semantic mapping\n"
        << "PASS unsupported SUMO states rejected\n"
        << "SUMO_SPATEM_SIGNAL_MAPPER_ACCEPTANCE=PASS\n";

    return 0;
}
