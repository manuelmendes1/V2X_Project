#include <cmath>
#include "simulation_controller.h"
#include "vanetza_integration/spatem/emitter/spatem_emitter_endpoint.h"
#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/packet/packet_id_manager.h"
#include "vanetza_integration/spatem/emitter/spatem_rsu_runtime.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_spatem_intersection_builder.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_timing_mapper.h"
#include <limits>
#include <memory>
#include "vanetza_integration/context/vehicle_network/vehicle_network_builder.h"

#include "vanetza_integration/cam/scheduler/cam_scheduler.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/simulation_step/simulation_step.h"
#include "vanetza_integration/context/simulation_sync/simulation_sync.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

#include "ns3/simulator.h"

#include <chrono>

#include <cstdint>
#include <iostream>
#include <utility>
#include <cstdlib>
#include <cerrno>

namespace ns3
{

Time SimulationController::m_stepLength = MilliSeconds(100);

bool SimulationController::m_denmEnabled = false;

double SimulationController::m_denmRadiusMeters = 1000.0;

DenmScenarioEventSource SimulationController::m_denmScenarioEvents{};

bool SimulationController::m_spatemEnabled = false;
bool SimulationController::m_spatemDirty = false;
uint32_t SimulationController::m_spatemStationId = 0;
uint16_t SimulationController::m_spatemIntersectionId = 0;
std::string SimulationController::m_spatemTrafficLightId;
double SimulationController::m_spatemLatitudeDeg = 0.0;
double SimulationController::m_spatemLongitudeDeg = 0.0;
double SimulationController::m_spatemNs3X = 0.0;
double SimulationController::m_spatemNs3Y = 0.0;
std::unique_ptr<SpatemEmitterEndpoint> SimulationController::m_spatemEmitter;
std::unique_ptr<SpatemRsuRuntime> SimulationController::m_spatemRuntime;
std::unique_ptr<SumoTrafficLightSpatemIntersectionBuilder> SimulationController::m_spatemIntersectionBuilder;


// ============================================================================
// Simulation lifecycle
// ============================================================================

void
SimulationController::Start(Time stepLength)
{
    m_stepLength = stepLength;

    // Polling SUMO mantém a sua cadência original.
    Simulator::ScheduleNow(
        &SimulationController::PollLoop);

    // CAM passa a ter um scheduler independente.
    // O offset inicial opcional destina-se a ensaios controlados;
    // sem configuração, o scheduler mantém o arranque imediato.
    const char* camInitialDelayEnv =
        std::getenv("V2X_CAM_INITIAL_DELAY_US");

    long long camInitialDelayUs = 0;

    if (camInitialDelayEnv != nullptr && *camInitialDelayEnv != '\0')
    {
        char* end = nullptr;
        errno = 0;

        const long long parsedDelayUs =
            std::strtoll(camInitialDelayEnv, &end, 10);

        if (errno == 0 &&
            end != camInitialDelayEnv &&
            *end == '\0' &&
            parsedDelayUs >= 0)
        {
            camInitialDelayUs = parsedDelayUs;
        }
        else
        {
            NS_LOG_UNCOND(
                "[CAM_SCHEDULER] invalid V2X_CAM_INITIAL_DELAY_US; "
                "using 0");
        }
    }

    const auto camScheduleBaseUs =
        Simulator::Now().GetMicroSeconds();

    NS_LOG_UNCOND(
        "[CAM_SCHEDULER] schedule_base_sim_us="
        << camScheduleBaseUs
        << " initial_delay_us=" << camInitialDelayUs
        << " first_due_sim_us="
        << (camScheduleBaseUs + camInitialDelayUs));

    if (camInitialDelayUs == 0)
    {
        Simulator::ScheduleNow(
            &SimulationController::SendCam);
    }
    else
    {
        NS_LOG_UNCOND(
            "[CAM_SCHEDULER] initial_delay_us="
            << camInitialDelayUs);

        Simulator::Schedule(
            MicroSeconds(camInitialDelayUs),
            &SimulationController::SendCam);
    }

    // DENM é explicitamente opt-in e não altera o comportamento CAM.
    if (m_denmEnabled)
    {
        DenmScheduler::Reset();
        DenmScheduler::SetCompatibilityFallbackEnabled(false);
        m_denmScenarioEvents.Clear();

        m_denmScenarioEvents.SetSubmissionTimingCallback(
            [](const DenmScenarioEvent& event,
               uint64_t triggerTimestampUs,
               uint64_t submissionTimestampUs,
               uint64_t)
            {
                std::cout
                    << "[DENM_TRIGGER_TIMING]"
                    << " vehicle=" << event.vehicleId
                    << " event=" << event.eventId
                    << " trigger_us=" << triggerTimestampUs
                    << " submit_sim_us=" << submissionTimestampUs
                    << '\n';
            });

        Simulator::ScheduleNow(
            &SimulationController::SendDenm);
    }
}

void
SimulationController::StopSimulation()
{
    Simulator::Stop();
}

void
SimulationController::ShutdownSpatemEmitter()
{
    const bool hadSpatemResources =
        static_cast<bool>(m_spatemRuntime) ||
        static_cast<bool>(m_spatemIntersectionBuilder) ||
        static_cast<bool>(m_spatemEmitter);

    // The runtime references the endpoint: destroy it first.
    m_spatemRuntime.reset();
    m_spatemIntersectionBuilder.reset();
    m_spatemEmitter.reset();
    m_spatemDirty = false;

    if (hadSpatemResources)
    {
        NS_LOG_UNCOND("[SPATEM_RSU] SHUTDOWN_COMPLETE");
    }
}

// ============================================================================
// Main polling loop
// ============================================================================

void
SimulationController::PollLoop()
{
    SumoAdapter::Poll();

    SendSpatemSnapshot();

    SimulationStep();

    Simulator::Schedule(
        m_stepLength,
        &SimulationController::PollLoop);
}

// ============================================================================
// Independent CAM scheduler
// ============================================================================

void SimulationController::SendCam() {
    static bool firstSchedulerFireLogged = false;

    if (!firstSchedulerFireLogged)
    {
        NS_LOG_UNCOND(
            "[CAM_SCHEDULER] first_sendcam_fire_sim_us="
            << Simulator::Now().GetMicroSeconds());

        firstSchedulerFireLogged = true;
    }

    CamScheduler::Run();

    Simulator::Schedule(
        CamScheduler::RunInterval(),
        &SimulationController::SendCam);
}

void
SimulationController::SetDenmEnabled(bool enabled)
{
    m_denmEnabled = enabled;
}

void
SimulationController::SetDenmRadiusMeters(double radiusMeters)
{
    m_denmRadiusMeters = radiusMeters;
}

void
SimulationController::SetDenmPkiDirectory(const std::string& directory)
{
    VehicleSecurityInitializer::SetDenmPkiDirectory(directory);
}


void
SimulationController::SendDenm()
{
    if (!m_denmEnabled)
    {
        return;
    }

    m_denmScenarioEvents.Evaluate(
        static_cast<uint64_t>(
            Simulator::Now().GetMicroSeconds()));

    DenmScheduler::Run(
        m_denmRadiusMeters);

    Simulator::Schedule(
        DenmScheduler::RunInterval(),
        &SimulationController::SendDenm);
}

// ============================================================================
// Simulation step
// ============================================================================

void
SimulationController::SimulationStep()
{
    if (!SimulationSync::HasPendingStep())
    {
        return;
    }

    /*
     * SimulationStep permanece responsável pela sincronização
     * SUMO -> ns-3 e pela exportação do estado.
     *
     * CamScheduler::Run() foi removido daqui porque agora possui
     * uma cadência temporal independente em SendCam().
     */
    SimulationStep::Execute();

    SimulationSync::ClearPendingStep();
}

// ============================================================================
// SUMO → ns-3 synchronization
// ============================================================================

void
SimulationController::ConfigureSpatemEmitter(
    bool enabled,
    uint32_t stationId,
    uint32_t intersectionId,
    const std::string& trafficLightId,
    double latitudeDeg,
    double longitudeDeg,
    double ns3X,
    double ns3Y)
{
    NS_ABORT_MSG_IF(
        enabled &&
            (stationId == 0 ||
             intersectionId == 0 ||
             intersectionId > 65535U ||
             trafficLightId.empty() ||
             !std::isfinite(latitudeDeg) ||
             latitudeDeg < -90.0 || latitudeDeg > 90.0 ||
             !std::isfinite(longitudeDeg) ||
             longitudeDeg < -180.0 || longitudeDeg > 180.0 ||
             !std::isfinite(ns3X) ||
             !std::isfinite(ns3Y)),
        "[SPATEM_RSU] Enabled SPATEM requires station/intersection IDs, "
        "trafficLightId and finite geographic/ns-3 physical coordinates");

    m_spatemEnabled = enabled;
    m_spatemStationId = stationId;
    m_spatemIntersectionId =
        static_cast<uint16_t>(intersectionId);
    m_spatemTrafficLightId = trafficLightId;
    m_spatemLatitudeDeg = latitudeDeg;
    m_spatemLongitudeDeg = longitudeDeg;
    m_spatemNs3X = ns3X;
    m_spatemNs3Y = ns3Y;

    m_spatemRuntime.reset();
    m_spatemIntersectionBuilder.reset();
    m_spatemEmitter.reset();

    NS_LOG_UNCOND(
        "[SPATEM_RSU] configuration enabled="
        << m_spatemEnabled
        << " station_id="
        << m_spatemStationId
        << " intersection_id="
        << m_spatemIntersectionId
        << " traffic_light="
        << m_spatemTrafficLightId
        << " latitude_deg=" << m_spatemLatitudeDeg
        << " longitude_deg=" << m_spatemLongitudeDeg
        << " ns3_x=" << m_spatemNs3X
        << " ns3_y=" << m_spatemNs3Y);
}

void
SimulationController::ConfigureSumoAdapterCallbacks()
{
    SumoAdapter::SetVehicleUpdateCallback(
        &SimulationController::HandleVehicleUpdateFromSumo);

    SumoAdapter::SetDenmEventCallback(
        &SimulationController::HandleDenmEventFromSumo);

    SumoAdapter::SetTrafficLightUpdateCallback(
        &SimulationController::HandleTrafficLightUpdateFromSumo);
}

void
SimulationController::HandleTrafficLightUpdateFromSumo(
    const SumoTrafficLightState& state)
{
    if (!m_spatemEnabled ||
        state.trafficLightId != m_spatemTrafficLightId)
    {
        return;
    }

    // Create the RSU endpoint and its runtime only once.
    // Later SUMO updates continue through the same endpoint.
    if (!m_spatemEmitter)
    {
        SpatemEmitterConfig config;
        config.logicalId = "spatem-rsu-" + m_spatemTrafficLightId;
        config.stationId = m_spatemStationId;
        config.trafficLightId = m_spatemTrafficLightId;
        config.intersectionId = m_spatemIntersectionId;
        config.latitudeDeg = m_spatemLatitudeDeg;
        config.longitudeDeg = m_spatemLongitudeDeg;
        config.ns3X = m_spatemNs3X;
        config.ns3Y = m_spatemNs3Y;

        auto endpoint = SpatemEmitterEndpoint::Create(config);
        if (!endpoint)
        {
            NS_LOG_UNCOND(
                "[SPATEM_RSU] ERROR: endpoint configuration rejected"
                << " traffic_light=" << m_spatemTrafficLightId);
            return;
        }

        if (!VehicleNetworkBuilder::InitializeSpatemEmitter(*endpoint))
        {
            NS_LOG_UNCOND(
                "[SPATEM_RSU] ERROR: network initialization failed"
                << " traffic_light=" << m_spatemTrafficLightId);
            return;
        }

        m_spatemEmitter = std::move(endpoint);

        auto runtime =
            std::make_unique<SpatemRsuRuntime>(*m_spatemEmitter);

        if (!runtime->Initialize())
        {
            NS_LOG_UNCOND(
                "[SPATEM_RSU] ERROR: GeoNetworking runtime initialization failed");
            runtime.reset();
            m_spatemEmitter.reset();
            return;
        }

        m_spatemRuntime = std::move(runtime);

        SumoSpatemIntersectionConfig intersectionConfig;
        intersectionConfig.trafficLightId =
            m_spatemEmitter->TrafficLightId();
        intersectionConfig.intersectionId =
            m_spatemEmitter->IntersectionId();
        intersectionConfig.revision =
            m_spatemEmitter->Revision();
        intersectionConfig.statusBits =
            m_spatemEmitter->StatusBits();
        intersectionConfig.region =
            m_spatemEmitter->Region();

        m_spatemIntersectionBuilder =
            std::make_unique<SumoTrafficLightSpatemIntersectionBuilder>(
                intersectionConfig);

        NS_LOG_UNCOND(
            "[SPATEM_RSU] ENDPOINT_READY"
            << " logical_id=" << m_spatemEmitter->LogicalId()
            << " station_id=" << m_spatemEmitter->StationId()
            << " node_id=" << m_spatemEmitter->GetNode()->GetId()
            << " ipv4=" << m_spatemEmitter->GetIpv4Address()
            << " link_mac=" << m_spatemEmitter->GetLinkMac()
            << " traffic_light=" << m_spatemEmitter->TrafficLightId());
    }

    if (!m_spatemRuntime ||
        !m_spatemRuntime->IsInitialized() ||
        !m_spatemIntersectionBuilder)
    {
        return;
    }

    if (!std::isfinite(state.simulationTime) ||
        state.simulationTime < 0.0)
    {
        NS_LOG_UNCOND(
            "[SPATEM_RSU] ERROR: invalid SUMO simulation time"
            << " traffic_light=" << state.trafficLightId
            << " time_s=" << state.simulationTime);
        return;
    }

    const long double elapsedUsExact =
        static_cast<long double>(state.simulationTime) * 1000000.0L;

    if (elapsedUsExact >= static_cast<long double>(
            std::numeric_limits<int64_t>::max()))
    {
        NS_LOG_UNCOND(
            "[SPATEM_RSU] ERROR: SUMO simulation time exceeds ITS clock range");
        return;
    }

    const int64_t elapsedUs =
        static_cast<int64_t>(std::llround(elapsedUsExact));

    SumoSpatemTimeReference reference;
    std::string error;

    if (!SumoTrafficLightTimingMapper::BuildReference(
            SimulationContext::itsStartTime,
            elapsedUs,
            reference,
            error))
    {
        NS_LOG_UNCOND(
            "[SPATEM_RSU] ERROR: time reference rejected: " << error);
        return;
    }

    if (!m_spatemIntersectionBuilder->Update(state, reference, error))
    {
        NS_LOG_UNCOND(
            "[SPATEM_RSU] ERROR: SUMO-to-SPATEM mapping failed"
            << " traffic_light=" << state.trafficLightId
            << " link_index=" << state.linkIndex
            << " raw_state=" << state.rawState
            << " reason=" << error);
        return;
    }

    // The poll loop sends one snapshot after all available UDP records
    // have updated the accumulated intersection state.
    m_spatemDirty = true;
}

void
SimulationController::SendSpatemSnapshot()
{
    if (!m_spatemEnabled ||
        !m_spatemDirty ||
        !m_spatemEmitter ||
        !m_spatemRuntime ||
        !m_spatemRuntime->IsInitialized() ||
        !m_spatemIntersectionBuilder)
    {
        return;
    }

    const auto& intersection =
        m_spatemIntersectionBuilder->GetIntersection();

    if (intersection.movements.empty())
    {
        return;
    }

    SpatemInput input;
    input.stationId = m_spatemEmitter->StationId();
    input.intersections.push_back(intersection);

    auto context = std::make_shared<PacketContext>();
    context->senderNodeId = m_spatemEmitter->GetNode()->GetId();
    context->senderStationId = m_spatemEmitter->StationId();
    context->packetId = PacketIdManager::Instance().NextPacketId();
    context->sequenceId =
        PacketIdManager::Instance().NextSequenceId(
            m_spatemEmitter->LogicalId(),
            PayloadType::SPATEM);

    const bool accepted =
        m_spatemRuntime->Send(context, input);

    // Keep the snapshot dirty when a send request is rejected so that
    // the next poll can retry the accumulated intersection state.
    if (accepted)
    {
        m_spatemDirty = false;
    }

    NS_LOG_UNCOND(
        "[SPATEM_RSU] TX_SNAPSHOT"
        << " result=" << (accepted ? "ACCEPTED" : "FAILED")
        << " packet_id=" << context->packetId
        << " station_id=" << context->senderStationId
        << " intersection_id=" << m_spatemIntersectionId
        << " movement_count=" << intersection.movements.size()
        << " sim_time_s=" << Simulator::Now().GetSeconds());
}

void
SimulationController::HandleVehicleUpdateFromSumo(
    const VehicleState& parsedVs)
{
    SimulationSync::ProcessVehicle(parsedVs);
}


void
SimulationController::HandleDenmEventFromSumo(
    const DenmScenarioEvent& event)
{
    const uint64_t ingestSimUs =
        static_cast<uint64_t>(
            Simulator::Now().GetMicroSeconds());

    const uint64_t ingestMonotonicUs =
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

    std::cout
        << "[DENM_EVENT_INGEST]"
        << " vehicle=" << event.vehicleId
        << " event=" << event.eventId
        << " trigger_us=" << event.triggerTimeUs
        << " ingest_sim_us=" << ingestSimUs
        << " ingest_monotonic_us=" << ingestMonotonicUs
        << '\n';

    m_denmScenarioEvents.AddEvent(event);
}

std::size_t
SimulationController::DenmScenarioEventCount()
{
    return m_denmScenarioEvents.EventCount();
}

std::size_t
SimulationController::PendingDenmScenarioEventCount()
{
    return m_denmScenarioEvents.PendingEventCount();
}

void
SimulationController::ResetDenmScenarioEvents()
{
    m_denmScenarioEvents.Clear();
}

} // namespace ns3
