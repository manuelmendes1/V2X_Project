#include "vanetza_integration/sumo_adapter/sumo_traffic_light_parser.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_timing_mapper.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

#include <iostream>
#include <string>

namespace
{

bool
Expect(bool condition, const std::string& message)
{
    if (condition)
    {
        std::cout << "PASS " << message << "\n";
        return true;
    }

    std::cout << "FAIL " << message << "\n";
    return false;
}

ns3::SumoTrafficLightState
MakeState(
    double simulationTime,
    double nextSwitch)
{
    ns3::SumoTrafficLightState state;

    state.trafficLightId = "B0";
    state.linkIndex = 0;
    state.rawState = 'G';
    state.phaseIndex = 0;
    state.phaseDuration = 30.0;
    state.nextSwitch = nextSwitch;
    state.programId = 1;
    state.simulationTime = simulationTime;

    return state;
}

} // namespace

int
main()
{
    std::cout
        << "===== SUMO SPATEM TIMING MAPPER ACCEPTANCE =====\n";

    bool ok = true;

    // Synthetic SPATEM clock reference:
    // simulation t=0 corresponds to minuteOfYear=120,
    // dSecond=10000 ms => 00:02:10.000 UTC-equivalent.
    ns3::SumoSpatemTimeReference reference;
    reference.minuteOfYear = 120;
    reference.dSecond = 10000;

    {
        auto input = MakeState(0.0, 30.0);

        SpatemTiming timing;
        std::string error;

        const bool mapped =
            ns3::SumoTrafficLightTimingMapper::Map(
                input,
                reference,
                timing,
                error);

        ok &= Expect(
            mapped,
            "future nextSwitch maps successfully");

        ok &= Expect(
            mapped && timing.minEndTime == 400,
            "nextSwitch=30s maps to TimeMark=400");

        ok &= Expect(
            mapped && !timing.maxEndTime.has_value(),
            "maxEndTime remains unspecified");

        ok &= Expect(
            mapped && !timing.likelyTime.has_value(),
            "likelyTime remains unspecified");

        ok &= Expect(
            mapped && !timing.confidence.has_value(),
            "confidence remains unspecified");
    }

    {
        // 1.1 s before the end of the UTC hour:
        // minute 119 + dSecond 59900 ms.
        // +1.2 s => next hour at 1.1 s => TimeMark 11.
        reference.minuteOfYear = 119;
        reference.dSecond = 59900;

        auto input = MakeState(5.0, 6.2);

        SpatemTiming timing;
        std::string error;

        const bool mapped =
            ns3::SumoTrafficLightTimingMapper::Map(
                input,
                reference,
                timing,
                error);

        ok &= Expect(
            mapped,
            "hour rollover maps successfully");

        ok &= Expect(
            mapped && timing.minEndTime == 11,
            "TimeMark wraps correctly across UTC hour boundary to 0.1s=11");
    }

    {
        auto input = MakeState(10.0, 9.0);

        SpatemTiming timing;
        std::string error;

        const bool mapped =
            ns3::SumoTrafficLightTimingMapper::Map(
                input,
                reference,
                timing,
                error);

        ok &= Expect(
            !mapped,
            "past nextSwitch is rejected");
    }

    if (ok)
    {
        std::cout
            << "SUMO_SPATEM_TIMING_MAPPER_ACCEPTANCE=PASS\n"
            << "SUMO_SPATEM_TIMING_MAPPER_GREEN_RC=0\n";
        return 0;
    }

    std::cout
        << "SUMO_SPATEM_TIMING_MAPPER_ACCEPTANCE=FAIL\n"
        << "SUMO_SPATEM_TIMING_MAPPER_GREEN_RC=1\n";

    return 1;
}
