#include "neighbor_manager.h"

#include "neighbor_logger.h"

namespace ns3
{

void NeighborManager::UpdateNeighbor(
    std::map<uint32_t, NeighborInfo>& table,
    uint32_t stationId,
    double x,
    double y,
    double speed,
    double heading,
    uint64_t step,
    const std::string& observerVehId)
{
    NeighborInfo& neighbor = table[stationId];

    neighbor.stationID = stationId;
    neighbor.x = x;
    neighbor.y = y;
    neighbor.speed = speed;
    neighbor.heading = heading;
    neighbor.lastUpdateStep = step;

    NeighborLogger::LogUpdate(
        observerVehId,
        neighbor,
        step);
}

void NeighborManager::PrintTable(
    const std::string& vehId,
    const std::map<uint32_t, NeighborInfo>& table,
    uint64_t step)
{
    NeighborLogger::LogTable(
        vehId,
        table,
        step);
}

void NeighborManager::PurgeOld(
    std::map<uint32_t, NeighborInfo>& table,
    uint64_t currentStep,
    uint64_t maxAge)
{
    for (auto it = table.begin(); it != table.end(); )
    {
        uint64_t age = currentStep - it->second.lastUpdateStep;

        if (age > maxAge)
        {
            NeighborLogger::LogPurge(
                std::to_string(it->first),
                it->first,
                age,
                currentStep);

            it = table.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace ns3
