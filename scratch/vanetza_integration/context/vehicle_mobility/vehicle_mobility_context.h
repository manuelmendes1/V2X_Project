#pragma once

#include "ns3/vector.h"

#include <optional>

namespace ns3
{

// Geographic WGS84 position. These coordinates are distinct from the
// projected/local ns-3 mobility position stored in Vector.
struct VehicleGeodeticPosition
{
    double longitudeDeg{};
    double latitudeDeg{};
};

struct VehicleMobilityContext
{
    Vector position;

    // Absent when the SUMO sample contains no WGS84 coordinates.
    std::optional<VehicleGeodeticPosition> geodeticPosition;

    double speed = 0.0;

    double heading = 0.0;

    double accel = 0.0;
};

} // namespace ns3
