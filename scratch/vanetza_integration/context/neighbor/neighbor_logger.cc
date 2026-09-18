#include "neighbor_logger.h"

#include "vanetza_integration/core/v2x_log.h"

namespace ns3
{

void NeighborLogger::LogUpdate(const std::string& observerVehId,
                               const NeighborInfo& neighbor,
                               uint64_t step)
{
    // Logging temporariamente desativado.
    (void)observerVehId;
    (void)neighbor;
    (void)step;
}

void NeighborLogger::LogTable(const std::string& vehId,
                              const std::map<uint32_t, NeighborInfo>& table,
                              uint64_t step)
{
    // Logging temporariamente desativado.
    (void)vehId;
    (void)table;
    (void)step;
}

void NeighborLogger::LogPurge(const std::string& observerVehId,
                              uint32_t stationId,
                              uint64_t age,
                              uint64_t step)
{
    // Logging temporariamente desativado.
    (void)observerVehId;
    (void)stationId;
    (void)age;
    (void)step;
}

} // namespace ns3
