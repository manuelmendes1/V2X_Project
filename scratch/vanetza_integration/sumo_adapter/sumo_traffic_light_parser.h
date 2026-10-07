#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace ns3 {

struct SumoTrafficLightState
{
    std::string trafficLightId;

    uint32_t linkIndex = 0;

    char rawState = '\0';

    uint32_t phaseIndex = 0;

    double phaseDuration = 0.0;

    double nextSwitch = 0.0;

    int32_t programId = 0;

    double simulationTime = 0.0;
};

class SumoTrafficLightParser
{
public:
    static std::unique_ptr<SumoTrafficLightState>
    Parse(const std::string& message);
};

} // namespace ns3
