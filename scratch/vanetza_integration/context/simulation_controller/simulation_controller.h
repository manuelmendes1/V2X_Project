#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "ns3/core-module.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"

namespace ns3
{

struct SumoTrafficLightState;
class SpatemEmitterEndpoint;
class SpatemRsuRuntime;
class SumoTrafficLightSpatemIntersectionBuilder;

class SimulationController
{
public:

    static void Start(Time stepLength);

    static void SetDenmEnabled(bool enabled);

    static void SetDenmRadiusMeters(double radiusMeters);

    static void SetDenmPkiDirectory(const std::string& directory);

    static void HandleVehicleUpdateFromSumo(const VehicleState& parsedVs);

    static void ConfigureSumoAdapterCallbacks();

    static void ConfigureSpatemEmitter(
        bool enabled,
        uint32_t stationId,
        uint32_t intersectionId,
        const std::string& trafficLightId,
        double latitudeDeg,
        double longitudeDeg,
        double ns3X,
        double ns3Y);

    static void HandleTrafficLightUpdateFromSumo(
        const SumoTrafficLightState& state);

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

    // Release SPATEM resources before ns-3 and static teardown.
    static void ShutdownSpatemEmitter();

private:

    static void SendSpatemSnapshot();

    static Time m_stepLength;

    static bool m_denmEnabled;

    static double m_denmRadiusMeters;

    static bool m_spatemEnabled;
    static uint32_t m_spatemStationId;
    static uint16_t m_spatemIntersectionId;
    static std::string m_spatemTrafficLightId;
    static double m_spatemLatitudeDeg;
    static double m_spatemLongitudeDeg;
    static double m_spatemNs3X;
    static double m_spatemNs3Y;
    static std::unique_ptr<SpatemEmitterEndpoint> m_spatemEmitter;
    static std::unique_ptr<SpatemRsuRuntime> m_spatemRuntime;
    static std::unique_ptr<SumoTrafficLightSpatemIntersectionBuilder>
        m_spatemIntersectionBuilder;

    static bool m_spatemDirty;

    static DenmScenarioEventSource m_denmScenarioEvents;
};

} // namespace ns3
