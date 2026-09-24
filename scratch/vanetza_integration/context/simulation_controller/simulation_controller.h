#pragma once

#include "ns3/core-module.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"

namespace ns3
{

class SimulationController
{
public:

    static void Start(Time stepLength);

    static void SetDenmEnabled(bool enabled);

    static void SetDenmRadiusMeters(double radiusMeters);

    static void SetDenmPkiDirectory(const std::string& directory);

    static void HandleVehicleUpdateFromSumo(const VehicleState& parsedVs);

    static void ConfigureSumoAdapterCallbacks();

    static void HandleDenmEventFromSumo(
        const DenmScenarioEvent& event);

    static std::size_t DenmScenarioEventCount();

    static std::size_t PendingDenmScenarioEventCount();

    static void ResetDenmScenarioEvents();

    static void SendCam();

    static void SendDenm();

    static void SimulationStep();

    static void PollLoop();

    static void StopSimulation();

private:

    static Time m_stepLength;

    static bool m_denmEnabled;

    static double m_denmRadiusMeters;

    static DenmScenarioEventSource m_denmScenarioEvents;
};

} // namespace ns3
