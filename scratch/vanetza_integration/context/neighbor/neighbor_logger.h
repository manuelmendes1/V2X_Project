#pragma once

#include <map>
#include <string>

#include "neighbor_types.h"

namespace ns3
{

class NeighborLogger
{
public:

    static void LogUpdate(
        const std::string& observerVehId,
        const NeighborInfo& neighbor,
        uint64_t step);

    static void LogTable(
        const std::string& vehId,
        const std::map<uint32_t, NeighborInfo>& table,
        uint64_t step);

    static void LogPurge(
        const std::string& observerVehId,
        uint32_t stationId,
        uint64_t age,
        uint64_t step);
};

} // namespace ns3
