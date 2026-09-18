#pragma once

#include <cstdint>

#include "ns3/nstime.h"

namespace ns3
{

class DenmScheduler
{
public:
    static void Run(
        double disseminationRadiusMeters);

    static Time RunInterval();

    static void Reset();
};

} // namespace ns3
