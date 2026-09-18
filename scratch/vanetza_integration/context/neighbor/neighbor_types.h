#pragma once

#include <cstdint>

namespace ns3 {

/**
 * ======================================================
 * NEIGHBOR INFORMATION (SINGLE SOURCE OF TRUTH)
 * ======================================================
 * Representa o estado de um veículo vizinho no sistema V2X.
 * Usado por:
 *  - NeighborManager
 *  - VehicleState
 *  - CAM receiver logic
 * ======================================================
 */
struct NeighborInfo
{
    uint32_t stationID;

    double x;
    double y;

    double speed;
    double heading;

    uint64_t lastUpdateStep;
};

} // namespace ns3
