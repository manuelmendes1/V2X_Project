#pragma once

#include <cstdint>
#include <optional>

struct VamPotiSample
{
    // WGS84 geodetic coordinates.
    std::optional<double> latitudeDeg;
    std::optional<double> longitudeDeg;
    std::optional<double> altitudeMeters;

    // Kinematics: SI units.
    std::optional<double> speedMps;

    // SUMO convention: degrees clockwise from North.
    // 360 degrees is accepted as equivalent to 0 degrees.
    std::optional<double> headingDeg;

    std::optional<double> longitudinalAccelerationMps2;

    // Logical SUMO simulation clock, never wall-clock time.
    std::optional<std::uint64_t> positionTimeSimulationUs;
    std::optional<std::uint64_t> generationTimeSimulationUs;
};
