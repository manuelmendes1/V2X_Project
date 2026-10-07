#include <iostream>
#include <string>

#include "vanetza_integration/sumo_adapter/sumo_traffic_light_parser.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_timing_mapper.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_spatem_mapper.h"

static bool Check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cout << "FAIL " << message << "\n";
        return false;
    }

    std::cout << "PASS " << message << "\n";
    return true;
}

int main()
{
    bool ok = true;

    ns3::SumoTrafficLightState input;
    input.trafficLightId = "B0";
    input.linkIndex = 1;
    input.rawState = 'G';
    input.phaseIndex = 0;
    input.phaseDuration = 30.0;
    input.nextSwitch = 30.0;
    input.programId = 1;
    input.simulationTime = 0.0;

    ns3::SumoSpatemTimeReference reference;
    reference.minuteOfYear = 120;
    reference.dSecond = 10000;

    SpatemMovement output;
    std::string error;

    const bool mapped =
        ns3::SumoTrafficLightSpatemMapper::Map(
            input,
            reference,
            output,
            error);

    ok &= Check(mapped,
                "combined SUMO TLS state+timing maps successfully");

    ok &= Check(output.signalGroup == 1,
                "signalGroup remains equal to linkIndex");

    ok &= Check(
        output.events.size() == 1,
        "exactly one SPATEM movement event is produced");

    if (!output.events.empty())
    {
        ok &= Check(
            output.events[0].phaseState ==
                SpatemMovementPhaseState::
                    PROTECTED_MOVEMENT_ALLOWED,
            "G maps to PROTECTED_MOVEMENT_ALLOWED");

        ok &= Check(
            output.events[0].timing.has_value(),
            "SPATEM event contains timing");

        if (output.events[0].timing)
        {
            ok &= Check(
                output.events[0].timing->minEndTime == 400,
                "nextSwitch=30s becomes minEndTime=400");

            ok &= Check(
                !output.events[0].timing->maxEndTime.has_value(),
                "maxEndTime remains unspecified");

            ok &= Check(
                !output.events[0].timing->likelyTime.has_value(),
                "likelyTime remains unspecified");

            ok &= Check(
                !output.events[0].timing->confidence.has_value(),
                "confidence remains unspecified");
        }
    }

    ok &= Check(
        error.empty(),
        "successful composition does not report an error");

    if (ok)
    {
        std::cout
            << "SUMO_SPATEM_SIGNAL_TIMING_MAPPER_ACCEPTANCE=PASS\n"
            << "SUMO_SPATEM_SIGNAL_TIMING_MAPPER_GREEN_RC=0\n";
        return 0;
    }

    std::cout
        << "SUMO_SPATEM_SIGNAL_TIMING_MAPPER_GREEN_RC=1\n";

    return 1;
}
