#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ns3/core-module.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"
#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"
#include "vanetza_integration/vam/cluster/vam_cluster_lifecycle_state_machine.h"
#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"
#include "vanetza_integration/vam/emitter/vam_vru_emitter_endpoint_store.h"
#include "vanetza_integration/vru/role/types/vru_role_types.h"

class CpmVehiclePerceptionSource;

namespace ns3
{

class CpmEmitterEndpoint;
class CpmRsuRuntime;
class VamVruEmitterEndpoint;

struct SumoTrafficLightState;
class SpatemEmitterEndpoint;
class SpatemRsuRuntime;
class SumoTrafficLightSpatemIntersectionBuilder;

struct SumoVehicleRemoveDatagram;

// Explicit scenario-owned VAM identity and role configuration.
// No field is inferred from SUMO IDs, vClass, ns-3 node IDs, or MAC addresses.
struct VamVruScenarioSubjectConfig
{
    using VbsState = vanetza_integration::vru::role::VbsState;

    std::string personId;
    std::uint32_t stationId{0};
    bool activateRole{false};
    VbsState vbsState{VbsState::Idle};
};

class SimulationController
{
public:

    static void Start(Time stepLength);

    static const SumoPersonRegistry& GetSumoPersonRegistry();

    static const SumoVruSubjectRegistry& GetSumoVruSubjectRegistry();

    // Explicit C++ management API. These operations are never inferred from
    // SUMO class, position, speed, or person ID.
    static std::optional<
        vanetza_integration::vru::role::VruTransitionResult>
    ActivateVruSubject(const std::string& personId);

    static std::optional<
        vanetza_integration::vru::role::VruTransitionResult>
    DeactivateVruSubject(const std::string& personId);

    static std::optional<
        vanetza_integration::vru::role::VruTransitionResult>
    SetVruSubjectVbsState(
        const std::string& personId,
        vanetza_integration::vru::role::VbsState state);

    static std::optional<
        vanetza_integration::vam::VruClusterLifecycleResult>
    ApplyVruSubjectClusterLifecycle(
        const std::string& personId,
        const vanetza_integration::vam::VruClusterLifecycleEvent& event);

    // Opt-in generation evaluation only. This does not encode, sign,
    // or transmit VAM packets.
    static bool ConfigureVamGeneration(
        bool enabled,
        const vanetza_integration::vam::VamGenerationConfig& generation,
        const vanetza_integration::vam::VamGenerationEvaluationAdapterConfig& evaluation,
        std::string& error,
        std::optional<ns3::SumoUtmWgs84ProjectionConfig> sumoProjection = std::nullopt,
        vanetza_integration::vam::VamGenerationAbortExecutor abortExecutor = {}); // CONTROLLER_ABORT_FAULT_INJECTION_V2

    // Explicit external identity assignment. Never derive stationId from
    // SUMO person ID, ns-3 Node ID, or MAC address.
    static bool RegisterVamVruStationIdentity(
        const std::string& personId,
        std::uint32_t stationId,
        std::string& error);

    // Atomically validates scenario-owned VRU assignments and reserves
    // their globally unique ITS station identities.
    static bool ConfigureVamVruScenarioSubjects(
        const std::vector<VamVruScenarioSubjectConfig>& subjects,
        std::string& error);

    static std::vector<
        vanetza_integration::vam::VamGenerationScheduledCandidate>
    GetPendingVamGenerationCandidates();

    // confirmedAtMs is logical SUMO simulation time, in milliseconds.
    static bool ConfirmVamGeneration(
        const vanetza_integration::vam::VamGenerationTransactionHandle& handle,
        std::int64_t confirmedAtMs,
        std::string& error);

    static bool AbortVamGeneration(
        const vanetza_integration::vam::VamGenerationTransactionHandle& handle,
        std::string& error);

    static void SetCamEnabled(bool enabled);

    static void SetDenmEnabled(bool enabled);

    static void SetDenmRadiusMeters(double radiusMeters);
    static void SetDenmInitialTxDelayUs(uint64_t delayUs);

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
    static void HandleVehicleRemoveFromSumo(const SumoVehicleRemoveDatagram& datagram);

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

    // Drain VAM transactions before releasing the scheduler. A failed
    // drain preserves the scheduler and blocks normal simulation teardown.
    static bool ShutdownVamGeneration();

private:

    static void DrainCpmReceptionQueues();
    static void SendSpatemSnapshot();
    static bool EnsureCpmRuntime();
    static void RunCpmPerception(std::uint64_t referenceTimeUs);
    static void RunVamGeneration(std::uint64_t referenceTimeUs);
    static void PruneRetiredVamVruEndpoints();
    static void HandleSumoStepComplete(std::uint64_t referenceTimeUs);
    static void HandlePersonUpdateFromSumo(const SumoPersonDatagram& datagram);
    static bool ApplyVamVruScenarioSubjectConfiguration(
        const std::string& personId);

    static Time m_stepLength;
    static SumoPersonRegistry m_sumoPersonRegistry;
    static SumoVruSubjectRegistry m_sumoVruSubjectRegistry;
    static std::unique_ptr<vanetza_integration::vam::VamGenerationScheduler> m_vamGenerationScheduler;
    static std::map<std::string, VamVruScenarioSubjectConfig>
        m_vamVruScenarioSubjects;
    // Active network resources are separate from scenario declarations and
    // the authoritative SUMO/person and derived VRU subject registries.
    static VamVruEmitterEndpointStore m_vamVruEndpointStore;

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
