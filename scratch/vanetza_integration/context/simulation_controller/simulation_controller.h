#pragma once

#include "ns3/core-module.h"

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

class SimulationController
{
public:

    static void Start(Time stepLength);

    static void SetDenmEnabled(bool enabled);

    static void SetDenmRadiusMeters(double radiusMeters);

    static void HandleVehicleUpdateFromSumo(const VehicleState& parsedVs);

    static void SendCam();

    static void SendDenm();

    static void SimulationStep();

    static void PollLoop();

    static void StopSimulation();

private:

    static Time m_stepLength;

    static bool m_denmEnabled;

    static double m_denmRadiusMeters;
};

} // namespace ns3
