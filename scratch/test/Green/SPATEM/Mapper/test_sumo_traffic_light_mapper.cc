#include <iostream>
#include <string>

#include "sumo_adapter/sumo_traffic_light_mapper.h"

namespace
{

bool CheckMapping(
    char rawState,
    SpatemMovementPhaseState expected,
    const char* name)
{
    ns3::SumoTrafficLightState input;
    input.trafficLightId = "tls0";
    input.linkIndex = 5;
    input.rawState = rawState;

    SpatemMovement output;
    output.signalGroup = 99;

    std::string error = "stale error";

    const bool accepted =
        ns3::SumoTrafficLightMapper::Map(input, output, error);

    const bool correct =
        accepted &&
        error.empty() &&
        output.signalGroup == 5 &&
        output.events.size() == 1 &&
        output.events[0].phaseState == expected;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << name << '\n';

    if (!correct)
    {
        std::cout << "  raw_state=" << rawState
                  << " accepted=" << accepted
                  << " error=" << error
                  << " signal_group="
                  << static_cast<unsigned>(output.signalGroup)
                  << " events=" << output.events.size()
                  << '\n';
    }

    return correct;
}

bool CheckUppercaseORejectedWithoutChangingOutput()
{
    ns3::SumoTrafficLightState input;
    input.trafficLightId = "tls0";
    input.linkIndex = 5;
    input.rawState = 'O';

    SpatemMovement output;
    output.signalGroup = 77;

    SpatemMovementEvent existingEvent;
    existingEvent.phaseState =
        SpatemMovementPhaseState::STOP_AND_REMAIN;
    output.events.push_back(existingEvent);

    std::string error;

    const bool accepted =
        ns3::SumoTrafficLightMapper::Map(input, output, error);

    const bool correct =
        !accepted &&
        error == "unsupported SUMO traffic-light state: O" &&
        output.signalGroup == 77 &&
        output.events.size() == 1 &&
        output.events[0].phaseState ==
            SpatemMovementPhaseState::STOP_AND_REMAIN;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << "uppercase_O_rejected_without_state_mutation\n";

    if (!correct)
    {
        std::cout << "  accepted=" << accepted
                  << " error=" << error
                  << " signal_group="
                  << static_cast<unsigned>(output.signalGroup)
                  << " events=" << output.events.size()
                  << '\n';
    }

    return correct;
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    const auto run = [&](bool result)
    {
        result ? ++passed : ++failed;
    };

    run(CheckMapping(
        'G',
        SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED,
        "uppercase_G_protected"));
    run(CheckMapping(
        'g',
        SpatemMovementPhaseState::PERMISSIVE_MOVEMENT_ALLOWED,
        "lowercase_g_permissive"));
    run(CheckMapping(
        'Y',
        SpatemMovementPhaseState::PROTECTED_CLEARANCE,
        "uppercase_Y_protected_clearance"));
    run(CheckMapping(
        'y',
        SpatemMovementPhaseState::PERMISSIVE_CLEARANCE,
        "lowercase_y_permissive_clearance"));
    run(CheckMapping(
        'R',
        SpatemMovementPhaseState::STOP_AND_REMAIN,
        "uppercase_R_stop"));
    run(CheckMapping(
        'r',
        SpatemMovementPhaseState::STOP_AND_REMAIN,
        "lowercase_r_stop"));

    run(CheckMapping(
        'o',
        SpatemMovementPhaseState::PERMISSIVE_MOVEMENT_ALLOWED,
        "lowercase_o_maps_to_permissive"));

    run(CheckUppercaseORejectedWithoutChangingOutput());

    std::cout << "SPATEM_SUMO_MAPPER_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_SUMO_MAPPER_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_SUMO_MAPPER_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_SUMO_MAPPER_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
