#include <cstdint>
#include <iostream>
#include <string>

#include "sumo_adapter/sumo_traffic_light_spatem_intersection_builder.h"

namespace
{

ns3::SumoTrafficLightState MakeState(uint32_t linkIndex)
{
    ns3::SumoTrafficLightState state;
    state.trafficLightId = "tls0";
    state.linkIndex = linkIndex;
    state.rawState = 'G';
    state.simulationTime = 1.0;
    state.nextSwitch = 3.0;
    return state;
}

bool CheckValidLinkAccepted()
{
    ns3::SumoSpatemIntersectionConfig config;
    config.trafficLightId = "tls0";
    config.intersectionId = 42;
    config.revision = 7;

    ns3::SumoTrafficLightSpatemIntersectionBuilder builder(config);

    ns3::SumoSpatemTimeReference reference;
    reference.minuteOfYear = 100;
    reference.dSecond = 1000;

    std::string error;
    const bool accepted =
        builder.Update(MakeState(7), reference, error);

    const auto& intersection = builder.GetIntersection();

    const bool correct =
        accepted &&
        error.empty() &&
        intersection.intersectionId == 42 &&
        intersection.revision == 7 &&
        intersection.movements.size() == 1 &&
        intersection.movements[0].signalGroup == 7 &&
        intersection.movements[0].events.size() == 1;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << "valid_link_index_accepted\n";
    return correct;
}

bool CheckInvalidLinkRejectedWithoutChangingState()
{
    ns3::SumoSpatemIntersectionConfig config;
    config.trafficLightId = "tls0";
    config.intersectionId = 42;
    config.revision = 7;

    ns3::SumoTrafficLightSpatemIntersectionBuilder builder(config);

    ns3::SumoSpatemTimeReference reference;
    reference.minuteOfYear = 100;
    reference.dSecond = 1000;

    std::string error;

    if (!builder.Update(MakeState(7), reference, error))
    {
        std::cerr
            << "[FAIL] link_256_rejected_and_state_preserved"
            << " reason=valid_baseline_update_failed error="
            << error << '\n';
        return false;
    }

    auto invalid = MakeState(256);
    invalid.rawState = 'R';

    error.clear();
    const bool accepted = builder.Update(invalid, reference, error);

    const auto& intersection = builder.GetIntersection();

    const bool correct =
        !accepted &&
        !error.empty() &&
        intersection.intersectionId == 42 &&
        intersection.revision == 7 &&
        intersection.movements.size() == 1 &&
        intersection.movements[0].signalGroup == 7 &&
        intersection.movements[0].events.size() == 1 &&
        intersection.movements[0].events[0].phaseState ==
            SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << "link_256_rejected_and_state_preserved\n";

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " error=" << error
                  << " movement_count=" << intersection.movements.size()
                  << '\n';
    }

    return correct;
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    if (CheckValidLinkAccepted())
        ++passed;
    else
        ++failed;

    if (CheckInvalidLinkRejectedWithoutChangingState())
        ++passed;
    else
        ++failed;

    std::cout << "SPATEM_SUMO_BOUNDARY_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_SUMO_BOUNDARY_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_SUMO_BOUNDARY_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_SUMO_BOUNDARY_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
