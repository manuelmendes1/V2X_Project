#pragma once

#include "ns3/vector.h"

namespace ns3
{

struct VehicleMobilityContext
{
    Vector position;

    double speed = 0.0;

    double heading = 0.0;

    double accel = 0.0;
};

} // namespace ns3
