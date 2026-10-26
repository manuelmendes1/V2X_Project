#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "ns3/core-module.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"

class CpmVehiclePerceptionSource;

namespace ns3
{

class CpmEmitterEndpoint;
class CpmRsuRuntime;

struct SumoTrafficLightState;
class SpatemEmitterEndpoint;
class SpatemRsuRuntime;
class SumoTrafficLightSpatemIntersectionBuilder;

class SimulationController
{
public:

    static void Start(Time stepLength);

    static void SetCamEnabled(bool enabled);

    static void SetDenmEnabled(bool enabled);

    static void SetDenmRadiusMeters(double radiusMeters);

    static void SetDenmPkiDirectory(const std::string& directory);

    // Constrói CpmInput a partir dos estados SUMO; não transmite pacotes.
    static bool ConfigureCpmPerception(
        bool enabled,
        uint32_t stationId,
        double latitudeDeg,
        double longitudeDeg,
        double ns3X,
        double ns3Y,
        double perceptionRangeMeters);

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

    // Release RSU runtimes and endpoints before Simulator::Destroy().
    static void ShutdownCpmEmitter();

    // Release SPATEM resources before ns-3 and static teardown.
    static void ShutdownSpatemEmitter();

private:

    static void DrainCpmReceptionQueues();
    static void SendSpatemSnapshot();
    static bool EnsureCpmRuntime();
    static void RunCpmPerception(std::uint64_t referenceTimeUs);
    static void HandleSumoStepComplete(std::uint64_t referenceTimeUs);

    static Time m_stepLength;

    static bool m_camEnabled;
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

    static bool m_cpmEnabled;
    static uint32_t m_cpmStationId;
    static double m_cpmLatitudeDeg;
    static double m_cpmLongitudeDeg;
    static double m_cpmNs3X;
    static double m_cpmNs3Y;
    static double m_cpmPerceptionRangeMeters;
    static std::unique_ptr<CpmEmitterEndpoint> m_cpmEmitter;
    static std::unique_ptr<CpmRsuRuntime> m_cpmRuntime;
    static int64_t m_cpmItsStartClockUs;
    static std::optional<std::uint64_t> m_cpmLastEncodedReferenceTimeUs;
    static std::unique_ptr<::CpmVehiclePerceptionSource>
        m_cpmPerceptionSource;
};

} // namespace ns3
