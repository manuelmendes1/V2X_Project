#pragma once

#include "ns3/core-module.h"

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

class SimulationController
{
public:

    static void Start(Time stepLength);

    static void HandleVehicleUpdateFromSumo(const VehicleState& parsedVs);

    static void SendCam();

    static void SimulationStep();

    static void PollLoop();

    static void StopSimulation();

private:

    static Time m_stepLength;
};

} // namespace ns3
