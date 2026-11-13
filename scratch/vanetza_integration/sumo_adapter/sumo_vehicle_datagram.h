#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ns3
{

struct SumoVehicleDatagram
{
    std::string vehId;
    double x{};
    double y{};
    double speed{};
    double heading{};
    double accel{};

    // Optional logical SUMO simulation time, in microseconds.
    // This is not an ITS absolute timestamp or a wall-clock timestamp.
    std::optional<std::uint64_t> simulationTimeUs;

    // Optional WGS84 geographic coordinates, in decimal degrees.
    // Both values are present only in the nine-field datagram format.
    std::optional<double> longitudeDeg;
    std::optional<double> latitudeDeg;
};

std::optional<SumoVehicleDatagram>
ParseSumoVehicleDatagram(const std::string& message);

struct SumoVehicleRemoveDatagram
{
    std::string vehId;
    std::uint64_t simulationTimeUs{};
};

std::optional<SumoVehicleRemoveDatagram>
ParseSumoVehicleRemoveDatagram(const std::string& message);

} // namespace ns3
