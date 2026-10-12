#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

namespace
{

struct Observation
{
    ns3::SumoTrafficLightState state;
};

bool HasObservation(
    const std::vector<Observation>& observations,
    uint32_t linkIndex,
    char rawState,
    uint32_t phaseIndex)
{
    return std::any_of(
        observations.begin(),
        observations.end(),
        [=](const Observation& obs)
        {
            return obs.state.trafficLightId == "B0" &&
                   obs.state.linkIndex == linkIndex &&
                   obs.state.rawState == rawState &&
                   obs.state.phaseIndex == phaseIndex;
        });
}

template <typename T>
void Expect(bool condition, const T& message, bool& ok)
{
    if (condition)
    {
        std::cout << "PASS " << message << "\n";
    }
    else
    {
        std::cout << "FAIL " << message << "\n";
        ok = false;
    }
}

} // namespace

int
main()
{
    constexpr int port = 7424;

    std::cout << "===== SUMO ADAPTER SPATEM TRACI ACCEPTANCE =====\n";

    bool ok = true;
    std::vector<Observation> observations;

    const bool initialized =
        SumoAdapter::Init(port, "127.0.0.1", 7000);

    Expect(initialized, "SUMO adapter initializes", ok);

    if (!initialized)
    {
        std::cout << "SUMO_ADAPTER_SPATEM_TRACI_RED_RC=1\n";
        return 1;
    }

    SumoAdapter::SetTrafficLightUpdateCallback(
        [&](const ns3::SumoTrafficLightState& state)
        {
            observations.push_back({state});
        });

    // Wait for an external real TraCI publisher.
    for (int i = 0; i < 500; ++i)
    {
        SumoAdapter::Poll();

        if (HasObservation(observations, 0, 'G', 0) &&
            HasObservation(observations, 1, 'G', 0) &&
            HasObservation(observations, 0, 'y', 1) &&
            HasObservation(observations, 1, 'y', 1))
        {
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    Expect(
        HasObservation(observations, 0, 'G', 0),
        "real TraCI TLS linkIndex 0 phase 0 reaches callback",
        ok);

    Expect(
        HasObservation(observations, 1, 'G', 0),
        "real TraCI TLS linkIndex 1 phase 0 reaches callback",
        ok);

    Expect(
        HasObservation(observations, 0, 'y', 1),
        "real TraCI TLS linkIndex 0 phase 1 reaches callback",
        ok);

    Expect(
        HasObservation(observations, 1, 'y', 1),
        "real TraCI TLS linkIndex 1 phase 1 reaches callback",
        ok);

    bool foundProgram1 = false;
    bool foundDuration30 = false;
    bool foundDuration10 = false;
    bool foundNextSwitch30 = false;
    bool foundNextSwitch40 = false;

    for (const auto& observation : observations)
    {
        const auto& state = observation.state;

        if (state.trafficLightId != "B0")
        {
            continue;
        }

        if (state.programId == 1)
        {
            foundProgram1 = true;
        }

        if (state.phaseIndex == 0 &&
            state.phaseDuration == 30.0)
        {
            foundDuration30 = true;
        }

        if (state.phaseIndex == 1 &&
            state.phaseDuration == 10.0)
        {
            foundDuration10 = true;
        }

        if (state.nextSwitch == 30.0)
        {
            foundNextSwitch30 = true;
        }

        if (state.nextSwitch == 40.0)
        {
            foundNextSwitch40 = true;
        }
    }

    Expect(foundProgram1, "programId=1 preserved", ok);
    Expect(foundDuration30, "phase 0 duration=30s preserved", ok);
    Expect(foundDuration10, "phase 1 duration=10s preserved", ok);
    Expect(foundNextSwitch30, "nextSwitch=30s preserved", ok);
    Expect(foundNextSwitch40, "nextSwitch=40s preserved", ok);

    Expect(
        observations.size() >= 4,
        "multiple real TraCI TLS observations received",
        ok);

    std::cout << "OBSERVATIONS=" << observations.size() << "\n";

    SumoAdapter::Close();

    if (ok)
    {
        std::cout << "SUMO_ADAPTER_SPATEM_TRACI_ACCEPTANCE=PASS\n";
        std::cout << "SUMO_ADAPTER_SPATEM_TRACI_GREEN_RC=0\n";
        return 0;
    }

    std::cout << "SUMO_ADAPTER_SPATEM_TRACI_RED_RC=1\n";
    return 1;
}
