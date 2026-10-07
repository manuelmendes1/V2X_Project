#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "vanetza_integration/sumo_adapter/sumo_adapter.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_spatem_mapper.h"

namespace
{

bool g_phase0Observed = false;
bool g_phase1Observed = false;
bool g_mappingFailed = false;

int g_observations = 0;

uint32_t
ExpectedTimeMark(const ns3::SumoTrafficLightState& state)
{
    constexpr uint64_t referenceMs =
        120ULL * 60000ULL + 10000ULL;

    const double deltaSeconds =
        std::max(0.0, state.nextSwitch - state.simulationTime);

    const uint64_t deltaMs =
        static_cast<uint64_t>(
            std::llround(deltaSeconds * 1000.0));

    const uint64_t withinHourMs =
        (referenceMs + deltaMs) % 3600000ULL;

    return static_cast<uint32_t>(withinHourMs / 100ULL);
}

void
OnTrafficLight(const ns3::SumoTrafficLightState& state)
{
    ++g_observations;

    if (state.linkIndex != 0)
    {
        return;
    }

    ns3::SumoSpatemTimeReference reference;
    reference.minuteOfYear = 120;
    reference.dSecond = 10000;

    SpatemMovement movement;
    std::string error;

    const bool mapped =
        ns3::SumoTrafficLightSpatemMapper::Map(
            state,
            reference,
            movement,
            error);

    if (!mapped)
    {
        std::cerr
            << "FAIL composition for real TraCI observation: "
            << error << "\n";
        g_mappingFailed = true;
        return;
    }

    if (movement.signalGroup != state.linkIndex ||
        movement.events.size() != 1 ||
        !movement.events[0].timing.has_value())
    {
        std::cerr
            << "FAIL structural composition for phase "
            << state.phaseIndex
            << " simulationTime=" << state.simulationTime
            << "\n";
        g_mappingFailed = true;
        return;
    }

    const auto& event = movement.events[0];

    if (event.timing->minEndTime != ExpectedTimeMark(state))
    {
        std::cerr
            << "FAIL timing composition phase="
            << state.phaseIndex
            << " simulationTime="
            << state.simulationTime
            << " nextSwitch="
            << state.nextSwitch
            << " expected="
            << ExpectedTimeMark(state)
            << " actual="
            << event.timing->minEndTime
            << "\n";
        g_mappingFailed = true;
        return;
    }

    if (!g_phase0Observed &&
        state.phaseIndex == 0 &&
        state.rawState == 'G')
    {
        const bool valid =
            event.phaseState ==
                SpatemMovementPhaseState::
                    PROTECTED_MOVEMENT_ALLOWED;

        if (valid)
        {
            g_phase0Observed = true;

            std::cout
                << "PASS real TraCI phase 0 G -> "
                << "signalGroup 0, "
                << "PROTECTED_MOVEMENT_ALLOWED, "
                << "timing=" << event.timing->minEndTime
                << "\n";
        }
        else
        {
            std::cerr
                << "FAIL real TraCI phase 0 semantic state\n";
            g_mappingFailed = true;
        }
    }

    if (!g_phase1Observed &&
        state.phaseIndex == 1 &&
        state.rawState == 'y')
    {
        const bool valid =
            event.phaseState ==
                SpatemMovementPhaseState::
                    PERMISSIVE_CLEARANCE;

        if (valid)
        {
            g_phase1Observed = true;

            std::cout
                << "PASS real TraCI phase 1 y -> "
                << "signalGroup 0, "
                << "PERMISSIVE_CLEARANCE, "
                << "timing=" << event.timing->minEndTime
                << "\n";
        }
        else
        {
            std::cerr
                << "FAIL real TraCI phase 1 semantic state\n";
            g_mappingFailed = true;
        }
    }
}

} // namespace

int
main()
{
    bool ok = true;

    constexpr int port = 7400;

    std::cout
        << "===== SUMO SPATEM TRACI COMPOSED ACCEPTANCE =====\n";

    SumoAdapter::SetTrafficLightUpdateCallback(OnTrafficLight);

    const bool initialized =
        SumoAdapter::Init(port, "127.0.0.1", 7000);

    if (!initialized)
    {
        std::cerr << "FAIL SumoAdapter initialization\n";
        return 1;
    }

    for (int i = 0; i < 500; ++i)
    {
        SumoAdapter::Poll();

        if (g_phase0Observed &&
            g_phase1Observed &&
            !g_mappingFailed)
        {
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    SumoAdapter::Close();

    ok &= g_observations > 0;
    ok &= g_phase0Observed;
    ok &= g_phase1Observed;
    ok &= !g_mappingFailed;

    std::cout
        << "OBSERVATIONS=" << g_observations << "\n"
        << "PHASE0_COMPOSED=" << (g_phase0Observed ? 1 : 0) << "\n"
        << "PHASE1_COMPOSED=" << (g_phase1Observed ? 1 : 0) << "\n"
        << "MAPPING_FAILED=" << (g_mappingFailed ? 1 : 0) << "\n";

    if (ok)
    {
        std::cout
            << "SUMO_SPATEM_TRACI_COMPOSED_ACCEPTANCE=PASS\n"
            << "SUMO_SPATEM_TRACI_COMPOSED_GREEN_RC=0\n";
        return 0;
    }

    std::cout
        << "SUMO_SPATEM_TRACI_COMPOSED_ACCEPTANCE=FAIL\n"
        << "SUMO_SPATEM_TRACI_COMPOSED_GREEN_RC=1\n";

    return 1;
}
