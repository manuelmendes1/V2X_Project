#pragma once

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

class SimulationSync
{
public:

    static void ProcessVehicle(const VehicleState& parsedVs);

    static void NotifyStepReceived();

    static bool HasPendingStep();

    static void ClearPendingStep();

private:

    static bool m_pendingStep;
};

} // namespace ns3
