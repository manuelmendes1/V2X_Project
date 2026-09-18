#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "neighbor_types.h"

namespace ns3
{

class NeighborManager
{
public:

    static void UpdateNeighbor(
        std::map<uint32_t, NeighborInfo>& table,
        uint32_t stationId,
        double x,
        double y,
        double speed,
        double heading,
        uint64_t step,
        const std::string& observerVehId);

    static void PrintTable(
        const std::string& vehId,
        const std::map<uint32_t, NeighborInfo>& table,
        uint64_t step);

    static void PurgeOld(
        std::map<uint32_t, NeighborInfo>& table,
        uint64_t currentStep,
        uint64_t maxAge);
};

} // namespace ns3
