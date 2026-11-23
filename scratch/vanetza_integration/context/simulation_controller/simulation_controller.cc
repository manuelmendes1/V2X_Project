#include <string>
#include <cmath>
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/sumo_person_vru_subject_synchronizer/sumo_person_vru_subject_synchronizer.h"
#include "vanetza_integration/vru/role/types/vru_role_types.h"
#include "vanetza_integration/cpm/types/cpm_message.h"
#include "simulation_controller.h"
#include "vanetza_integration/cpm/perception/cpm_vehicle_perception_source.h"
#include "vanetza_integration/cpm/emitter/cpm_emitter_endpoint.h"
#include "vanetza_integration/vam/emitter/vam_vru_emitter_endpoint.h"
#include "vanetza_integration/cpm/emitter/cpm_rsu_runtime.h"
#include "vanetza_integration/context/packet/packet_id_manager.h"
#include "vanetza_integration/context/vehicle_dispatcher/vehicle_dispatcher.h"
#include <vanetza/common/clock.hpp>
#include <optional>
#include <vector>
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
#include "vanetza_integration/context/its_time/its_start_time_parser.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"
#include "vanetza_integration/context/simulation_step/simulation_step.h"
#include "vanetza_integration/context/simulation_sync/simulation_sync.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"
#include "vanetza_integration/vam/codec/vam_uper_encoder.h"

#include "ns3/simulator.h"

#include <chrono>

#include <cstdint>
#include <iostream>
#include <set>
#include <utility>
#include <cstdlib>
#include <cerrno>

namespace ns3
{

namespace
{

const char*
VruProfileName(
    vanetza_integration::vru::role::VruProfile profile) noexcept
{
    using vanetza_integration::vru::role::VruProfile;

    switch (profile)
    {
    case VruProfile::Unknown:
        return "Unknown";
    case VruProfile::Pedestrian:
        return "Pedestrian";
    case VruProfile::BicyclistLightVruVehicle:
        return "BicyclistLightVruVehicle";
    case VruProfile::Motorcyclist:
        return "Motorcyclist";
    case VruProfile::Animal:
        return "Animal";
    }

    return "Invalid";
}

} // namespace

Time SimulationController::m_stepLength = MilliSeconds(100);
SumoPersonRegistry SimulationController::m_sumoPersonRegistry{};
SumoVruSubjectRegistry SimulationController::m_sumoVruSubjectRegistry{};
std::unique_ptr<vanetza_integration::vam::VamGenerationScheduler>
    SimulationController::m_vamGenerationScheduler;
std::map<std::string, VamVruScenarioSubjectConfig>
    SimulationController::m_vamVruScenarioSubjects{};
VamVruEmitterEndpointStore
    SimulationController::m_vamVruEndpointStore{};

bool SimulationController::m_camEnabled = true;
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

bool SimulationController::m_cpmEnabled = false;
uint32_t SimulationController::m_cpmStationId = 0;
double SimulationController::m_cpmLatitudeDeg =
    std::numeric_limits<double>::quiet_NaN();
double SimulationController::m_cpmLongitudeDeg =
    std::numeric_limits<double>::quiet_NaN();
double SimulationController::m_cpmNs3X = 0.0;
double SimulationController::m_cpmNs3Y = 0.0;
double SimulationController::m_cpmPerceptionRangeMeters = 50.0;
std::unique_ptr<CpmEmitterEndpoint> SimulationController::m_cpmEmitter;
std::unique_ptr<CpmRsuRuntime> SimulationController::m_cpmRuntime;
int64_t SimulationController::m_cpmItsStartClockUs = 0;
std::optional<std::uint64_t> SimulationController::m_cpmLastEncodedReferenceTimeUs;
std::unique_ptr<::CpmVehiclePerceptionSource>
    SimulationController::m_cpmPerceptionSource;


// ============================================================================
// Simulation lifecycle
// ============================================================================

void
SimulationController::Start(Time stepLength)
{
    m_stepLength = stepLength;
    m_sumoPersonRegistry.Clear();
    m_sumoVruSubjectRegistry.Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    // Polling SUMO mantém a sua cadência original.
    Simulator::ScheduleNow(
        &SimulationController::PollLoop);

    // CAM passa a ter um scheduler independente.
    // O offset inicial opcional destina-se a ensaios controlados;
    // sem configuração, o scheduler mantém o arranque imediato.
    if (m_camEnabled)
    {
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
SimulationController::RunCpmPerception(std::uint64_t referenceTimeUs)
{
    if (!m_cpmEnabled || !m_cpmPerceptionSource)
    {
        return;
    }

    std::vector<CpmVehiclePerceptionObservation> observations;

    VehicleDispatcher::ForEachReadyVehicle(
        [&](VehicleState& vehicle)
        {
            std::optional<CpmGeodeticHorizontalPosition> position;

            if (vehicle.mobility.geodeticPosition.has_value())
            {
                const auto& geo = *vehicle.mobility.geodeticPosition;

                // VehicleMobilityContext guarda longitude antes de latitude;
                // a API geodésica CPM recebe latitude antes de longitude.
                position = CpmGeodeticHorizontalPosition{
                    geo.latitudeDeg,
                    geo.longitudeDeg};
            }

            observations.push_back(
                CpmVehiclePerceptionObservation{
                    vehicle.identity.vehId,
                    position,
                    vehicle.sumoSimulationTimeUs});

        });

    if (observations.empty())
    {
        return;
    }

    if (referenceTimeUs >
        static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] REJECTED reason=REFERENCE_TIME_OUT_OF_RANGE");
        return;
    }

    if (m_cpmLastEncodedReferenceTimeUs.has_value() &&
        referenceTimeUs <= *m_cpmLastEncodedReferenceTimeUs)
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] SKIPPED"
            << " reference_sumo_us=" << referenceTimeUs
            << " reason=STALE_OR_DUPLICATE_TIMESTAMP");
        return;
    }

    const CpmVehiclePerceptionConfig config{
        m_cpmStationId,
        m_cpmLatitudeDeg,
        m_cpmLongitudeDeg,
        m_cpmPerceptionRangeMeters,
        m_cpmItsStartClockUs};

    CpmInput input{};
    std::string error;

    if (!m_cpmPerceptionSource->Build(
            config,
            static_cast<int64_t>(referenceTimeUs),
            observations,
            input,
            error))
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] BUILD_REJECTED"
            << " reference_sumo_us=" << referenceTimeUs
            << " observed_vehicles=" << observations.size()
            << " reason=" << error);
        return;
    }

    // The current UPER profile requires at least one perceived object.
    // An empty perception result is an intentional skip, not a TX failure.
    if (input.perceivedObjects.empty())
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] TX_SKIPPED"
            << " reference_sumo_us=" << referenceTimeUs
            << " observed_vehicles=" << observations.size()
            << " known_objects="
            << static_cast<unsigned>(input.knownObjectCount)
            << " reason=NO_PERCEIVED_OBJECTS");
        return;
    }

    NS_LOG_UNCOND(
        "[CPM_PERCEPTION] BUILD_ACCEPTED"
        << " reference_sumo_us=" << referenceTimeUs
        << " reference_time_its_ms=" << input.referenceTimeItsMs
        << " observed_vehicles=" << observations.size()
        << " known_objects=" << static_cast<unsigned>(input.knownObjectCount)
        << " perceived_objects=" << input.perceivedObjects.size()
        << " mode=TX_READY");

    if (!EnsureCpmRuntime())
    {
        NS_LOG_UNCOND(
            "[CPM_RSU] TX_REJECTED reason=RUNTIME_NOT_READY"
            << " reference_sumo_us=" << referenceTimeUs);
        return;
    }

    auto context = std::make_shared<PacketContext>();
    context->simulationStep =
        static_cast<int64_t>(SimulationContext::simulationStep);
    context->simulationTime = Simulator::Now().GetSeconds();
    context->timestamp =
        static_cast<uint64_t>(Simulator::Now().GetMicroSeconds());

    // CPM is emitted by its dedicated RSU, not by a perceived vehicle.
    context->senderNodeId = m_cpmEmitter->GetNode()->GetId();
    context->senderStationId = m_cpmEmitter->StationId();
    context->packetId = PacketIdManager::Instance().NextPacketId();
    context->sequenceId = PacketIdManager::Instance().NextSequenceId(
        m_cpmEmitter->LogicalId(),
        PayloadType::CPM);

    // The selected traffic class is an explicit local configuration
    // choice; this value is not asserted to be a normative default.
    constexpr uint8_t cpmTrafficClass = 0;

    const bool sent =
        m_cpmRuntime->Send(context, input, cpmTrafficClass);

    // Preserve the original anti-duplicate gate once UPER bytes exist,
    // even if a subsequent transport step rejects the request.
    if (context->payload.type == PayloadType::CPM &&
        !context->payload.bytes.empty())
    {
        m_cpmLastEncodedReferenceTimeUs = referenceTimeUs;
    }

    NS_LOG_UNCOND(
        "[CPM_RSU] TX_RESULT"
        << " result=" << (sent ? "ACCEPTED" : "FAILED")
        << " packet_id=" << context->packetId
        << " station_id=" << context->senderStationId
        << " sequence_id=" << context->sequenceId
        << " btp_port=2009"
        << " its_aid=639"
        << " bytes=" << context->payload.bytes.size()
        << " perceived_objects=" << input.perceivedObjects.size()
        << " sim_time_s=" << Simulator::Now().GetSeconds());

}


void
SimulationController::DrainCpmReceptionQueues()
{
    const auto& vehicles =
        VehicleRegistry::Instance().GetAllVehicles();

    for (const auto& [vehicleId, vehiclePtr] : vehicles)
    {
        if (!vehiclePtr)
        {
            continue;
        }

        VehicleState& receiver = *vehiclePtr;

        if (!receiver.runtime ||
            !receiver.runtime->IsInitialized())
        {
            continue;
        }

        ::CpmMessage message{};

        while (receiver.runtime->TryReceiveCpm(message))
        {
            NS_LOG_UNCOND(
                "[CPM_APP_RX] DELIVER_OK"
                << " receiver=" << vehicleId
                << " receiver_station_id="
                << receiver.identity.stationId
                << " sender_station_id="
                << message.station.stationId
                << " known_objects="
                << static_cast<unsigned>(message.knownObjectCount)
                << " included_objects="
                << message.perceivedObjects.size()
                << " received_at_unix_ms="
                << message.receivedAtUnixMs);

            for (const auto& object : message.perceivedObjects)
            {
                NS_LOG_UNCOND(
                    "[CPM_APP_RX_OBJECT]"
                    << " receiver=" << vehicleId
                    << " sender_station_id="
                    << message.station.stationId
                    << " object_id=" << object.objectId
                    << " measurement_delta_time_ms="
                    << object.measurementDeltaTimeMs
                    << " x_east_m=" << object.position.xEast.value
                    << " y_north_m=" << object.position.yNorth.value);
            }
        }
    }
}

void
SimulationController::PollLoop()
{
    SumoAdapter::Poll();

    SendSpatemSnapshot();

    SimulationStep();

    // Consume validated CPM messages on the existing simulation cadence.
    DrainCpmReceptionQueues();

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

bool
SimulationController::EnsureCpmRuntime()
{
    if (m_cpmRuntime && m_cpmRuntime->IsInitialized())
    {
        return true;
    }

    // The endpoint owns an independent RSU identity and physical position.
    if (!m_cpmEmitter)
    {
        CpmEmitterConfig config;
        config.logicalId = "cpm-rsu-" + std::to_string(m_cpmStationId);
        config.stationId = m_cpmStationId;
        config.latitudeDeg = m_cpmLatitudeDeg;
        config.longitudeDeg = m_cpmLongitudeDeg;
        config.ns3X = m_cpmNs3X;
        config.ns3Y = m_cpmNs3Y;

        auto endpoint = CpmEmitterEndpoint::Create(config);
        if (!endpoint)
        {
            NS_LOG_UNCOND(
                "[CPM_RSU] ERROR endpoint configuration rejected"
                << " station_id=" << m_cpmStationId);
            return false;
        }

        if (!VehicleNetworkBuilder::InitializeCpmEmitter(*endpoint))
        {
            NS_LOG_UNCOND(
                "[CPM_RSU] ERROR network initialization failed"
                << " station_id=" << m_cpmStationId);
            return false;
        }

        m_cpmEmitter = std::move(endpoint);

        NS_LOG_UNCOND(
            "[CPM_RSU] ENDPOINT_READY"
            << " logical_id=" << m_cpmEmitter->LogicalId()
            << " station_id=" << m_cpmEmitter->StationId()
            << " node_id=" << m_cpmEmitter->GetNode()->GetId()
            << " ipv4=" << m_cpmEmitter->GetIpv4Address()
            << " link_mac=" << m_cpmEmitter->GetLinkMac()
            << " latitude_deg=" << m_cpmEmitter->LatitudeDeg()
            << " longitude_deg=" << m_cpmEmitter->LongitudeDeg()
            << " ns3_x=" << m_cpmNs3X
            << " ns3_y=" << m_cpmNs3Y);
    }

    auto runtime = std::make_unique<CpmRsuRuntime>(*m_cpmEmitter);
    if (!runtime->Initialize())
    {
        runtime.reset();
        m_cpmEmitter.reset();
        NS_LOG_UNCOND(
            "[CPM_RSU] ERROR runtime initialization failed"
            << " station_id=" << m_cpmStationId);
        return false;
    }

    m_cpmRuntime = std::move(runtime);
    return true;
}

void
SimulationController::ShutdownCpmEmitter()
{
    const bool hadResources =
        static_cast<bool>(m_cpmRuntime) ||
        static_cast<bool>(m_cpmEmitter);

    // Runtime depends on the endpoint; destroy it first.
    m_cpmRuntime.reset();
    m_cpmEmitter.reset();

    if (hadResources)
    {
        NS_LOG_UNCOND("[CPM_RSU] SHUTDOWN_COMPLETE");
    }
}

bool
SimulationController::ConfigureCpmPerception(
    bool enabled,
    uint32_t stationId,
    double latitudeDeg,
    double longitudeDeg,
    double ns3X,
    double ns3Y,
    double perceptionRangeMeters)
{
    // Desativar primeiro impede que uma configuração inválida deixe ativa
    // uma instância anterior da fonte.
    m_cpmEnabled = false;
    m_cpmPerceptionSource.reset();
    ShutdownCpmEmitter();

    if (!enabled)
    {
        NS_LOG_UNCOND("[CPM_PERCEPTION] DISABLED");
        return true;
    }

    if (stationId == 0 ||
        !std::isfinite(latitudeDeg) ||
        latitudeDeg < -90.0 || latitudeDeg > 90.0 ||
        !std::isfinite(longitudeDeg) ||
        longitudeDeg < -180.0 || longitudeDeg > 180.0 ||
        !std::isfinite(ns3X) ||
        !std::isfinite(ns3Y) ||
        !std::isfinite(perceptionRangeMeters) ||
        perceptionRangeMeters < 0.0)
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] ERROR invalid RSU ID, geography or range");
        return false;
    }

    if (SimulationContext::itsStartTime.empty())
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] ERROR enabled CPM requires --itsStartTime");
        return false;
    }

    int64_t startClockUs = 0;
    vanetza::Clock::time_point start;
    std::string parseError;

    if (!ItsStartTimeParser::ParseUtc(
            SimulationContext::itsStartTime,
            start,
            parseError))
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] ERROR invalid UTC --itsStartTime: "
            << SimulationContext::itsStartTime
            << " reason=" << parseError);
        return false;
    }

    const long double value =
        static_cast<long double>(start.time_since_epoch().count());

    if (!std::isfinite(value) ||
        value < 0.0L ||
        value > static_cast<long double>(
            std::numeric_limits<int64_t>::max()))
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] ERROR ITS clock is out of range");
        return false;
    }

    startClockUs = static_cast<int64_t>(value);

    const ItsStationIdentityOwner stationOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "cpm-rsu-" + std::to_string(stationId)
    };

    auto& identityAuthority =
        ItsStationIdentityAuthority::Instance();

    if (!identityAuthority.Reserve(stationOwner, stationId))
    {
        NS_LOG_UNCOND(
            "[CPM_PERCEPTION] ERROR global stationId reservation failed"
            << " logical_id=" << stationOwner.logicalId
            << " station_id=" << stationId);
        return false;
    }

    m_cpmLastEncodedReferenceTimeUs.reset();
    m_cpmStationId = stationId;
    m_cpmLatitudeDeg = latitudeDeg;
    m_cpmLongitudeDeg = longitudeDeg;
    m_cpmNs3X = ns3X;
    m_cpmNs3Y = ns3Y;
    m_cpmPerceptionRangeMeters = perceptionRangeMeters;
    m_cpmItsStartClockUs = startClockUs;
    m_cpmPerceptionSource =
        std::make_unique<::CpmVehiclePerceptionSource>();
    m_cpmEnabled = true;

    NS_LOG_UNCOND(
        "[CPM_PERCEPTION] CONFIGURED"
        << " station_id=" << stationId
        << " latitude_deg=" << latitudeDeg
        << " longitude_deg=" << longitudeDeg
        << " ns3_x=" << ns3X
        << " ns3_y=" << ns3Y
        << " range_m=" << perceptionRangeMeters
        << " its_start_clock_us=" << startClockUs
        << " mode=TX_ENABLED");

    return true;
}


bool
SimulationController::ConfigureVamGeneration(
    bool enabled,
    const vanetza_integration::vam::VamGenerationConfig& generation,
    const vanetza_integration::vam::VamGenerationEvaluationAdapterConfig& evaluation,
    std::string& error,
    std::optional<ns3::SumoUtmWgs84ProjectionConfig> sumoProjection,
    vanetza_integration::vam::VamGenerationAbortExecutor abortExecutor)
{
    if (!enabled)
    {
        // A scheduler may be destroyed only after every pending or retired
        // transaction has been resolved. Failure preserves the old instance.
        if (m_vamGenerationScheduler)
        {
            if (!m_vamGenerationScheduler->DrainTransactions(error))
            {
                error = "VAM disable blocked by unresolved transactions: " + error;
                NS_LOG_UNCOND(
                    "[VAM_GENERATION] DISABLE_BLOCKED reason=" << error);
                return false;
            }

            m_vamGenerationScheduler.reset();
        }

        PruneRetiredVamVruEndpoints();
        error.clear();
        NS_LOG_UNCOND("[VAM_GENERATION] DISABLED");
        return true;
    }

    // Validate the requested replacement without disturbing the active
    // scheduler. A rejected reconfiguration must preserve current runtime.
    if (SimulationContext::itsStartTime.empty())
    {
        error = "VAM generation requires --itsStartTime";
        NS_LOG_UNCOND("[VAM_GENERATION] CONFIGURATION_REJECTED reason=" << error);
        return false;
    }

    vanetza::Clock::time_point start;
    if (!ItsStartTimeParser::ParseUtc(
            SimulationContext::itsStartTime,
            start,
            error))
    {
        error = "Invalid UTC --itsStartTime: " + error;
        NS_LOG_UNCOND("[VAM_GENERATION] CONFIGURATION_REJECTED reason=" << error);
        return false;
    }

    const auto startClockUs = start.time_since_epoch().count();
    if (startClockUs < 0)
    {
        error = "VAM ITS start clock must be non-negative";
        NS_LOG_UNCOND("[VAM_GENERATION] CONFIGURATION_REJECTED reason=" << error);
        return false;
    }

    const auto validThresholds =
        [](const vanetza_integration::vam::VamProximityThresholds& thresholds)
        {
            return std::isfinite(thresholds.minimumSafeLateralDistanceMeters) &&
                   thresholds.minimumSafeLateralDistanceMeters > 0.0 &&
                   std::isfinite(thresholds.minimumSafeLongitudinalDistanceMeters) &&
                   thresholds.minimumSafeLongitudinalDistanceMeters > 0.0 &&
                   std::isfinite(thresholds.minimumSafeVerticalDistanceMeters) &&
                   thresholds.minimumSafeVerticalDistanceMeters > 0.0;
        };

    if (!vanetza_integration::vam::IsValidVamGenerationConfig(generation) ||
        !validThresholds(evaluation.snapshotBuilder.vehicleThresholds) ||
        !validThresholds(evaluation.snapshotBuilder.vruThresholds))
    {
        error =
            "VAM generation parameters must satisfy the generation manager "
            "contract, and all vehicle/VRU proximity thresholds must be "
            "explicitly configured with valid positive values";
        NS_LOG_UNCOND("[VAM_GENERATION] CONFIGURATION_REJECTED reason=" << error);
        return false;
    }

    vanetza_integration::vam::VamGenerationSchedulerConfig config{};
    config.itsStartClockUs = startClockUs;
    config.generation = generation;
    config.evaluation = evaluation;
    config.sumoProjection = std::move(sumoProjection);

    // Build the replacement first. Construction failure or validation
    // rejection cannot destroy the currently active scheduler.
    auto replacementScheduler =
        std::make_unique<
            vanetza_integration::vam::VamGenerationScheduler>(
                std::move(config),
                std::move(abortExecutor));

    // Exhaustion must fail closed. Keep the active scheduler untouched.
    if (!replacementScheduler->HasValidInstanceIdentity())
    {
        error = "VAM scheduler instance identifier is exhausted";
        NS_LOG_UNCOND(
            "[VAM_GENERATION] CONFIGURATION_REJECTED reason=" << error);
        return false;
    }

    // Drain the old instance only after the replacement has been fully
    // validated. A failed drain leaves the active scheduler untouched.
    if (m_vamGenerationScheduler)
    {
        if (!m_vamGenerationScheduler->DrainTransactions(error))
        {
            error = "VAM replacement blocked by unresolved transactions: " + error;
            NS_LOG_UNCOND(
                "[VAM_GENERATION] RECONFIGURATION_BLOCKED reason=" << error);
            return false;
        }
    }

    // Identity lookup is owned by ItsStationIdentityAuthority, not by the
    // scheduler. Scenario declarations remain owned by this controller.
    // swap is noexcept; the previous, fully drained scheduler is released afterwards.
    m_vamGenerationScheduler.swap(replacementScheduler);
    // The previous scheduler was drained before the swap; its retired
    // endpoint ownership can now be released safely.
    PruneRetiredVamVruEndpoints();

    error.clear();

    NS_LOG_UNCOND(
        "[VAM_GENERATION] CONFIGURED"
        << " its_start_clock_us=" << startClockUs
        << " t_check_ms=" << generation.tCheckVamGenMs
        << " t_gen_min_ms=" << generation.tGenVamMinMs
        << " t_gen_max_ms=" << generation.tGenVamMaxMs
        << " maximum_sample_age_us="
        << evaluation.proximity.maximumSampleAgeUs
        << " mode=EVALUATION_ONLY");

    return true;
}


bool
SimulationController::RegisterVamVruStationIdentity(
    const std::string& personId,
    std::uint32_t stationId,
    std::string& error)
{
    if (!m_vamGenerationScheduler)
    {
        error = "VAM generation scheduler is not enabled";
        return false;
    }

    return m_vamGenerationScheduler->RegisterSubjectStationIdentity(
        personId,
        vanetza_integration::vam::VamStationId{stationId},
        error);
}

bool
SimulationController::ConfigureVamVruScenarioSubjects(
    const std::vector<VamVruScenarioSubjectConfig>& subjects,
    std::string& error)
{
    using VbsState = VamVruScenarioSubjectConfig::VbsState;

    // An empty scenario list is valid even when VAM evaluation is disabled.
    // Existing ITS identity reservations are deliberately not released.
    if (subjects.empty())
    {
        std::map<std::string, VamVruScenarioSubjectConfig> empty;
        m_vamVruScenarioSubjects.swap(empty);
        error.clear();

        NS_LOG_UNCOND(
            "[VAM_SCENARIO] SUBJECTS_CONFIGURED"
            << " count=0 mode=NO_SUBJECTS");

        return true;
    }

    // Validate the complete declaration before reserving any identity.
    std::set<std::string> personIds;
    std::set<std::uint32_t> stationIds;
    std::map<std::string, VamVruScenarioSubjectConfig> staged;

    for (const auto& subject : subjects)
    {
        if (subject.personId.empty())
        {
            error = "VAM scenario personId must not be empty";
            return false;
        }

        if (subject.stationId == 0)
        {
            error = "VAM scenario stationId must be non-zero";
            return false;
        }

        if (!personIds.insert(subject.personId).second)
        {
            error = "Duplicate VAM scenario personId: " + subject.personId;
            return false;
        }

        if (!stationIds.insert(subject.stationId).second)
        {
            error = "Duplicate VAM scenario stationId: " +
                    std::to_string(subject.stationId);
            return false;
        }

        // Scenario startup supports only the non-cluster role states.
        // Cluster leadership/passive transitions must be driven through
        // ApplyClusterLifecycle(), never assigned through SetVbsState().
        switch (subject.vbsState)
        {
        case VbsState::Idle:
        case VbsState::ActiveStandalone:
            break;
        default:
            error =
                "Unsupported VBS state in VAM scenario configuration "
                "(only Idle and ActiveStandalone are supported): " +
                subject.personId;
            return false;
        }

        if (!subject.activateRole && subject.vbsState != VbsState::Idle)
        {
            error =
                "A VAM scenario subject with role activation disabled "
                "must use VbsState::Idle: " + subject.personId;
            return false;
        }

        if (subject.activateRole &&
            subject.vbsState != VbsState::ActiveStandalone)
        {
            error =
                "A VAM scenario subject with role activation enabled "
                "must use VbsState::ActiveStandalone: " + subject.personId;
            return false;
        }

        staged.emplace(subject.personId, subject);
    }

    if (!m_vamGenerationScheduler)
    {
        error =
            "VAM scenario subjects require an enabled VAM generation scheduler";
        return false;
    }

    auto& authority = ItsStationIdentityAuthority::Instance();

    // Only reservations created by this call are eligible for rollback.
    // Existing reservations for the same logical owner/ID remain intact.
    std::vector<std::pair<ItsStationIdentityOwner, std::uint32_t>>
        newlyReserved;

    const auto rollbackNewReservations = [&authority, &newlyReserved]()
    {
        for (const auto& reservation : newlyReserved)
        {
            authority.Release(reservation.first, reservation.second);
        }
    };

    // Reserve every identity before touching the scheduler's person map.
    // This catches global collisions before any scheduler registration.
    for (const auto& [personId, subject] : staged)
    {
        const ItsStationIdentityOwner owner{
            ItsStationIdentityOwnerKind::ItsStation,
            "vam-vru:" + personId
        };

        const auto existingStationId = authority.FindStationId(owner);

        if (existingStationId.has_value() &&
            *existingStationId != subject.stationId)
        {
            error =
                "VAM scenario cannot change an existing stationId reservation "
                "for personId: " + personId;
            rollbackNewReservations();
            return false;
        }

        if (!authority.Reserve(owner, subject.stationId))
        {
            error =
                "VAM scenario stationId reservation failed or conflicts "
                "with another ITS station: " + std::to_string(subject.stationId);
            rollbackNewReservations();
            return false;
        }

        if (!existingStationId.has_value())
        {
            newlyReserved.emplace_back(owner, subject.stationId);
        }
    }

    // The same owner/ID pairs are already reserved above. These registrations
    // populate the scheduler's person-to-station lookup.
    for (const auto& [personId, subject] : staged)
    {
        if (!RegisterVamVruStationIdentity(
                personId, subject.stationId, error))
        {
            // This is an invariant failure: validated, pre-reserved pairs
            // should be idempotently accepted by the scheduler.
            rollbackNewReservations();
            return false;
        }
    }

    // Publish configuration only after all reservations and registrations.
    m_vamVruScenarioSubjects.swap(staged);
    error.clear();

    NS_LOG_UNCOND(
        "[VAM_SCENARIO] SUBJECTS_CONFIGURED"
        << " count=" << m_vamVruScenarioSubjects.size()
        << " identities_reserved=" << newlyReserved.size());

    return true;
}

std::vector<
    vanetza_integration::vam::VamGenerationScheduledCandidate>
SimulationController::GetPendingVamGenerationCandidates()
{
    if (!m_vamGenerationScheduler)
    {
        return {};
    }

    return m_vamGenerationScheduler->GetPendingCandidates();
}

bool
SimulationController::ConfirmVamGeneration(
    const vanetza_integration::vam::VamGenerationTransactionHandle& handle,
    std::int64_t confirmedAtMs,
    std::string& error)
{
    if (!m_vamGenerationScheduler)
    {
        error = "VAM generation scheduler is not enabled";
        return false;
    }

    return m_vamGenerationScheduler->ConfirmGeneration(
        handle,
        confirmedAtMs,
        error);
}

bool
SimulationController::AbortVamGeneration(
    const vanetza_integration::vam::VamGenerationTransactionHandle& handle,
    std::string& error)
{
    if (!m_vamGenerationScheduler)
    {
        error = "VAM generation scheduler is not enabled";
        return false;
    }

    return m_vamGenerationScheduler->AbortGeneration(
        handle,
        error);
}

bool
SimulationController::ShutdownVamGeneration()
{
    if (!m_vamGenerationScheduler)
    {
        m_vamVruEndpointStore.ClearAll();
        return true;
    }

    std::string error;
    if (!m_vamGenerationScheduler->DrainTransactions(error))
    {
        NS_LOG_UNCOND(
            "[VAM_GENERATION] SHUTDOWN_BLOCKED reason=" << error);
        return false;
    }

    m_vamGenerationScheduler.reset();
    // Endpoint destruction unregisters IPv4 station mappings but deliberately
    // does not release the simulation-scoped ITS station identity.
    m_vamVruEndpointStore.ClearAll();
    NS_LOG_UNCOND("[VAM_GENERATION] SHUTDOWN_COMPLETE");
    return true;
}

void
SimulationController::SetCamEnabled(bool enabled)
{
    m_camEnabled = enabled;
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
SimulationController::SetDenmInitialTxDelayUs(uint64_t delayUs)
{
    DenmScheduler::SetInitialTransmissionDelayUs(delayUs);
}

void
SimulationController::SetDenmPkiDirectory(const std::string& directory)
{
    VehicleSecurityInitializer::SetDenmPkiDirectory(directory);
}


bool
SimulationController::ConfigureVehiclePki(
    const std::string& runDirectory,
    const std::string& mappingFile,
    std::string& error)
{
    return VehicleSecurityInitializer::ConfigureVehiclePki(
        runDirectory,
        mappingFile,
        error);
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

    if (enabled)
    {
        const ItsStationIdentityOwner stationOwner{
            ItsStationIdentityOwnerKind::ItsStation,
            "spatem-rsu-" + trafficLightId
        };

        auto& identityAuthority =
            ItsStationIdentityAuthority::Instance();

        NS_ABORT_MSG_IF(
            !identityAuthority.Reserve(stationOwner, stationId),
            "[SPATEM_RSU] Global stationId reservation failed"
                << " logical_id=" << stationOwner.logicalId
                << " station_id=" << stationId);
    }

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

    SumoAdapter::SetVehicleRemoveCallback(
        &SimulationController::HandleVehicleRemoveFromSumo);

    SumoAdapter::SetPersonUpdateCallback(
        &SimulationController::HandlePersonUpdateFromSumo);

    SumoAdapter::SetDenmEventCallback(
        &SimulationController::HandleDenmEventFromSumo);

    SumoAdapter::SetTrafficLightUpdateCallback(
        &SimulationController::HandleTrafficLightUpdateFromSumo);

    SumoAdapter::SetStepCompleteCallback(
        &SimulationController::HandleSumoStepComplete);
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
    auto& lifecycle = SumoVehicleLifecycleRegistry::Instance();

    if (!lifecycle.ApplyUpsert(
            parsedVs.identity.vehId,
            parsedVs.sumoSimulationTimeUs))
    {
        NS_LOG_UNCOND(
            "[SUMO_VEHICLE] UPSERT_REJECTED"
            << " vehicle_id=" << parsedVs.identity.vehId
            << " simulation_time_us="
            << (parsedVs.sumoSimulationTimeUs.has_value()
                    ? std::to_string(*parsedVs.sumoSimulationTimeUs)
                    : std::string("UNAVAILABLE")));
        return;
    }

    // Only an accepted SUMO lifecycle event may update persistent mobility.
    // VehicleState and its ns-3 resources remain owned by VehicleRegistry.
    SimulationSync::ProcessVehicle(parsedVs);
}

void
SimulationController::HandleVehicleRemoveFromSumo(
    const SumoVehicleRemoveDatagram& datagram)
{
    auto& lifecycle = SumoVehicleLifecycleRegistry::Instance();

    if (!lifecycle.ApplyRemove(
            datagram.vehId,
            datagram.simulationTimeUs))
    {
        NS_LOG_UNCOND(
            "[SUMO_VEHICLE] REMOVE_REJECTED"
            << " vehicle_id=" << datagram.vehId
            << " simulation_time_us=" << datagram.simulationTimeUs);
        return;
    }

    // This closes SUMO presence only. Do not erase VehicleState, Node,
    // Socket, runtime, identity indexes, or in-flight receive queues.
    NS_LOG_UNCOND(
        "[SUMO_VEHICLE] REMOVE_ACCEPTED"
        << " vehicle_id=" << datagram.vehId
        << " simulation_time_us=" << datagram.simulationTimeUs);
}

void
SimulationController::HandlePersonUpdateFromSumo(
    const SumoPersonDatagram& datagram)
{
    // A missing active person means this accepted UPSERT starts a new
    // presence period. This is also true after a previously accepted REMOVE.
    const auto previousPerson =
        m_sumoPersonRegistry.Find(datagram.personId);
    const bool startsNewPresence =
        datagram.eventType == SumoPersonEventType::Upsert &&
        !previousPerson.has_value();

    if (!m_sumoPersonRegistry.Apply(datagram))
    {
        NS_LOG_UNCOND(
            "[SUMO_PERSON] EVENT_REJECTED"
            << " person_id=" << datagram.personId
            << " event_type="
            << (datagram.eventType == SumoPersonEventType::Upsert
                    ? "UPSERT"
                    : "REMOVE_OR_UNKNOWN")
            << " simulation_time_us=" << datagram.simulationTimeUs);
        return;
    }

    // A failed invalidation is fail-closed: retain the endpoint while a
    // scheduler transaction remains quarantined or unresolved.
    bool vamPresenceInvalidated = true;

    // An accepted REMOVE terminates the VAM presence before the derived
    // VRU subject registry is synchronized. Abort failure is fail-closed:
    // the scheduler retains the transaction and quarantines only this person.
    if (datagram.eventType == SumoPersonEventType::Remove)
    {
        if (m_vamGenerationScheduler)
        {
            std::string vamInvalidationError;
            if (!m_vamGenerationScheduler->InvalidatePersonPresence(
                    datagram.personId,
                    vamInvalidationError))
            {
                vamPresenceInvalidated = false;
                NS_LOG_UNCOND(
                    "[VAM_GENERATION] CANDIDATE_ABORT_FAILED"
                    << " person_id=" << datagram.personId
                    << " error=" << vamInvalidationError
                    << " mode=EVALUATION_ONLY");
            }
        }
    }

    // PersonRegistry remains authoritative. Only accepted events reach the
    // derived VRU subject synchronizer, independently of optional audit logs.
    if (!SumoVruSubjectSynchronizer::SynchronizeAccepted(
            datagram, m_sumoVruSubjectRegistry))
    {
        NS_LOG_UNCOND(
            "[SUMO_VRU_SUBJECT] SYNCHRONIZATION_REJECTED"
            << " person_id=" << datagram.personId
            << " simulation_time_us=" << datagram.simulationTimeUs);
        return;
    }

    // Apply scenario-owned role/VBS settings exactly once per active
    // presence period, after the authoritative registries are synchronized.
    if (startsNewPresence &&
        !ApplyVamVruScenarioSubjectConfiguration(datagram.personId))
    {
        NS_LOG_UNCOND(
            "[VAM_SCENARIO] SUBJECT_CONFIGURATION_REJECTED"
            << " person_id=" << datagram.personId
            << " simulation_time_us=" << datagram.simulationTimeUs);
        return;
    }

    // Only scenario-declared people own a VAM VRU network endpoint. Retire
    // the exact endpoint presence that ended; the scheduler's ledger, not the
    // InvalidatePersonPresence() return value alone, decides whether it lives on.
    if (datagram.eventType == SumoPersonEventType::Remove)
    {
        auto* activeEndpoint =
            m_vamVruEndpointStore.FindActive(datagram.personId);
        if (activeEndpoint != nullptr)
        {
            const auto endpointEpoch = activeEndpoint->PresenceEpoch();
            const bool schedulerRetainsRuntime =
                m_vamGenerationScheduler &&
                m_vamGenerationScheduler->HasRetainedRuntimeForPresence(
                    datagram.personId, endpointEpoch);
            // A failed invalidation is fail-closed for this event. A later
            // scheduler boundary may prune it if no runtime was actually kept.
            const bool retainEndpoint =
                !vamPresenceInvalidated || schedulerRetainsRuntime;

            if (m_vamVruEndpointStore.RetireActive(
                    datagram.personId, retainEndpoint))
            {
                NS_LOG_UNCOND(
                    "[VAM_VRU_ENDPOINT] REMOVE_ACCEPTED"
                    << " person_id=" << datagram.personId
                    << " presence_epoch_us=" << endpointEpoch.value
                    << " runtime_retained="
                    << (retainEndpoint ? "yes" : "no")
                    << " station_identity_preserved=yes");
            }
            else
            {
                NS_LOG_UNCOND(
                    "[VAM_VRU_ENDPOINT] REMOVE_OWNERSHIP_REJECTED"
                    << " person_id=" << datagram.personId
                    << " presence_epoch_us=" << endpointEpoch.value);
            }
        }
    }
    else
    {
        const auto scenarioIt =
            m_vamVruScenarioSubjects.find(datagram.personId);
        if (scenarioIt != m_vamVruScenarioSubjects.end())
        {
            const auto personState =
                m_sumoPersonRegistry.Find(datagram.personId);
            if (!personState.has_value())
            {
                NS_LOG_UNCOND(
                    "[VAM_VRU_ENDPOINT] UPSERT_DEFERRED"
                    << " person_id=" << datagram.personId
                    << " reason=PERSON_SAMPLE_NOT_AVAILABLE");
            }
            else
            {
                const auto& sample = personState->sample;
                constexpr double kPi =
                    3.141592653589793238462643383279502884;
                const double headingRad = sample.angleDeg * kPi / 180.0;
                const Vector position(sample.x, sample.y, 0.0);
                // SUMO heading is degrees clockwise from North; ns-3 x is
                // East and y is North. Convert the complete sample atomically.
                const Vector velocity(
                    sample.speed * std::sin(headingRad),
                    sample.speed * std::cos(headingRad),
                    0.0);

                const vanetza_integration::vam::VamPersonPresenceEpoch
                    currentPresenceEpoch{personState->firstObservedAtUs};
                auto* activeEndpoint =
                    m_vamVruEndpointStore.FindActive(datagram.personId);
                bool ownershipReady = true;

                // Reconcile on every UPSERT. Do not use startsNewPresence as
                // the endpoint ownership test: later samples in a new epoch
                // must never update the endpoint of an older presence.
                if (activeEndpoint != nullptr &&
                    activeEndpoint->PresenceEpoch() != currentPresenceEpoch)
                {
                    const bool retainOldRuntime =
                        m_vamGenerationScheduler &&
                        m_vamGenerationScheduler->HasRetainedRuntimeForPresence(
                            datagram.personId, activeEndpoint->PresenceEpoch());
                    ownershipReady =
                        m_vamVruEndpointStore.ReconcileActivePresence(
                            datagram.personId,
                            currentPresenceEpoch,
                            retainOldRuntime);
                    if (!ownershipReady)
                    {
                        NS_LOG_UNCOND(
                            "[VAM_VRU_ENDPOINT] PRESENCE_RECONCILIATION_REJECTED"
                            << " person_id=" << datagram.personId
                            << " current_presence_epoch_us="
                            << currentPresenceEpoch.value);
                    }
                    activeEndpoint =
                        m_vamVruEndpointStore.FindActive(datagram.personId);
                }

                if (ownershipReady && activeEndpoint != nullptr)
                {
                    if (!activeEndpoint->UpdateMobility(position, velocity))
                    {
                        NS_LOG_UNCOND(
                            "[VAM_VRU_ENDPOINT] MOBILITY_UPDATE_REJECTED"
                            << " person_id=" << datagram.personId
                            << " presence_epoch_us="
                            << currentPresenceEpoch.value
                            << " simulation_time_us=" << datagram.simulationTimeUs);
                    }
                }
                else if (ownershipReady)
                {
                    VamVruEmitterConfig endpointConfig{};
                    endpointConfig.personId = datagram.personId;
                    endpointConfig.stationId = scenarioIt->second.stationId;
                    endpointConfig.presenceEpoch = currentPresenceEpoch;
                    endpointConfig.initialPosition = position;
                    endpointConfig.initialVelocity = velocity;

                    auto endpoint =
                        VamVruEmitterEndpoint::Create(endpointConfig);
                    if (!endpoint)
                    {
                        NS_LOG_UNCOND(
                            "[VAM_VRU_ENDPOINT] CREATE_REJECTED"
                            << " person_id=" << datagram.personId
                            << " station_id=" << endpointConfig.stationId
                            << " presence_epoch_us="
                            << currentPresenceEpoch.value);
                    }
                    else if (!VehicleNetworkBuilder::InitializeVamVruEmitter(
                                 *endpoint))
                    {
                        NS_LOG_UNCOND(
                            "[VAM_VRU_ENDPOINT] NETWORK_INITIALIZATION_FAILED"
                            << " person_id=" << datagram.personId
                            << " station_id=" << endpointConfig.stationId
                            << " presence_epoch_us="
                            << currentPresenceEpoch.value);
                    }
                    else if (!m_vamVruEndpointStore.InsertActive(
                                 std::move(endpoint)))
                    {
                        NS_LOG_UNCOND(
                            "[VAM_VRU_ENDPOINT] ACTIVE_OWNERSHIP_INSERT_REJECTED"
                            << " person_id=" << datagram.personId
                            << " presence_epoch_us="
                            << currentPresenceEpoch.value);
                    }
                    else
                    {
                        const auto* insertedEndpoint =
                            m_vamVruEndpointStore.FindActive(datagram.personId);
                        NS_LOG_UNCOND(
                            "[VAM_VRU_ENDPOINT] ENDPOINT_READY"
                            << " person_id=" << datagram.personId
                            << " logical_id=" << insertedEndpoint->LogicalId()
                            << " station_id=" << insertedEndpoint->StationId()
                            << " presence_epoch_us="
                            << insertedEndpoint->PresenceEpoch().value
                            << " node_id=" << insertedEndpoint->GetNode()->GetId()
                            << " ipv4=" << insertedEndpoint->GetIpv4Address());
                    }
                }
            }
        }
    }

    // Runtime audit is opt-in to avoid changing normal simulation logging.
    const char* auditEnv = std::getenv("V2X_SUMO_PERSON_AUDIT");
    if (auditEnv == nullptr || auditEnv[0] != '1' || auditEnv[1] != '\0')
    {
        return;
    }

    const auto subjectSnapshot =
        m_sumoVruSubjectRegistry.Find(datagram.personId);

    const char* profileName =
        subjectSnapshot.has_value()
            ? VruProfileName(subjectSnapshot->profile)
            : "NotPresent";

    const std::string sumoVClass =
        datagram.sumoVClass.value_or("none");

    const char* eventType =
        datagram.eventType == SumoPersonEventType::Upsert ? "UPSERT" : "REMOVE";

    std::cout
        << "[SUMO_PERSON_AUDIT] ACCEPTED"
        << " event=" << eventType
        << " person_id=" << datagram.personId
        << " simulation_time_us=" << datagram.simulationTimeUs
        << " active_count=" << m_sumoPersonRegistry.Size()
        << " sumo_vclass=" << sumoVClass
        << " vru_profile=" << profileName
        << '\n';
}

const SumoPersonRegistry&
SimulationController::GetSumoPersonRegistry()
{
    return m_sumoPersonRegistry;
}

const SumoVruSubjectRegistry&
SimulationController::GetSumoVruSubjectRegistry()
{
    return m_sumoVruSubjectRegistry;
}

std::optional<
    vanetza_integration::vru::role::VruTransitionResult>
SimulationController::ActivateVruSubject(const std::string& personId)
{
    return m_sumoVruSubjectRegistry.Activate(personId);
}

std::optional<
    vanetza_integration::vru::role::VruTransitionResult>
SimulationController::DeactivateVruSubject(const std::string& personId)
{
    return m_sumoVruSubjectRegistry.Deactivate(personId);
}

std::optional<
    vanetza_integration::vru::role::VruTransitionResult>
SimulationController::SetVruSubjectVbsState(
    const std::string& personId,
    vanetza_integration::vru::role::VbsState state)
{
    return m_sumoVruSubjectRegistry.SetVbsState(personId, state);
}

bool
SimulationController::ApplyVamVruScenarioSubjectConfiguration(
    const std::string& personId)
{
    using VbsState = VamVruScenarioSubjectConfig::VbsState;
    using VruTransitionResult =
        vanetza_integration::vru::role::VruTransitionResult;

    const auto configured = m_vamVruScenarioSubjects.find(personId);
    if (configured == m_vamVruScenarioSubjects.end())
    {
        return true;
    }

    const auto& subject = configured->second;

    if (!subject.activateRole)
    {
        // Configuration validation requires Idle when role activation is off.
        return subject.vbsState == VbsState::Idle;
    }

    const auto roleTransition = ActivateVruSubject(personId);
    if (!roleTransition.has_value() ||
        *roleTransition == VruTransitionResult::Rejected)
    {
        NS_LOG_UNCOND(
            "[VAM_SCENARIO] ROLE_ACTIVATION_REJECTED"
            << " person_id=" << personId);
        return false;
    }

    const auto vbsTransition =
        SetVruSubjectVbsState(personId, subject.vbsState);

    if (!vbsTransition.has_value() ||
        *vbsTransition == VruTransitionResult::Rejected)
    {
        // Fail closed if the second transition unexpectedly fails.
        DeactivateVruSubject(personId);

        NS_LOG_UNCOND(
            "[VAM_SCENARIO] VBS_STATE_REJECTED"
            << " person_id=" << personId);
        return false;
    }

    NS_LOG_UNCOND(
        "[VAM_SCENARIO] SUBJECT_ACTIVATED"
        << " person_id=" << personId
        << " station_id=" << subject.stationId
        << " vbs_state=" << static_cast<unsigned int>(subject.vbsState));

    return true;
}

std::optional<
    vanetza_integration::vam::VruClusterLifecycleResult>
SimulationController::ApplyVruSubjectClusterLifecycle(
    const std::string& personId,
    const vanetza_integration::vam::VruClusterLifecycleEvent& event)
{
    return m_sumoVruSubjectRegistry.ApplyClusterLifecycle(personId, event);
}


void
SimulationController::PruneRetiredVamVruEndpoints()
{
    const std::size_t released = m_vamVruEndpointStore.PruneRetired(
        [](const std::string& personId,
           vanetza_integration::vam::VamPersonPresenceEpoch presenceEpoch)
        {
            return m_vamGenerationScheduler &&
                m_vamGenerationScheduler->HasRetainedRuntimeForPresence(
                    personId, presenceEpoch);
        });

    if (released != 0)
    {
        NS_LOG_UNCOND(
            "[VAM_VRU_ENDPOINT] RETIRED_ENDPOINTS_PRUNED"
            << " released=" << released);
    }
}


void
SimulationController::RunVamGeneration(std::uint64_t referenceTimeUs)
{
    if (!m_vamGenerationScheduler)
    {
        return;
    }

    using namespace vanetza_integration::vam;

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};
    std::string error;

    const bool evaluationAccepted = m_vamGenerationScheduler->EvaluateStep(
        referenceTimeUs,
        m_sumoPersonRegistry,
        m_sumoVruSubjectRegistry,
        VehicleRegistry::Instance(),
        SumoVehicleLifecycleRegistry::Instance(),
        context,
        candidates,
        report,
        error);

    // EvaluateStep may recover a retired abort before returning either a
    // cadence skip or a later evaluation error. Reconcile endpoint ownership
    // before every early return.
    PruneRetiredVamVruEndpoints();

    if (!evaluationAccepted)
    {
        NS_LOG_UNCOND(
            "[VAM_GENERATION] STEP_REJECTED"
            << " reference_sumo_us=" << referenceTimeUs
            << " reason=" << error);
        return;
    }

    if (report.skippedByCadence)
    {
        return;
    }

    NS_LOG_UNCOND(
        "[VAM_GENERATION] STEP_EVALUATED"
        << " reference_sumo_us=" << referenceTimeUs
        << " evaluated=" << report.evaluatedSubjects
        << " candidates=" << candidates.size()
        << " blocked_by_policy=" << report.blockedByPolicy
        << " missing_identity=" << report.skippedMissingIdentity
        << " missing_subject=" << report.skippedMissingSubject
        << " pending=" << report.skippedPendingTransactions);

    for (const auto& candidate : candidates)
    {
        if (!candidate.decision.transactionId.has_value())
        {
            continue;
        }

        const auto transactionId =
            *candidate.decision.transactionId;

        NS_LOG_UNCOND(
            "[VAM_GENERATION] CANDIDATE_EVALUATED"
            << " person_id=" << candidate.egoPersonId
            << " station_id=" << candidate.stationId.value
            << " transaction_id=" << transactionId
            << " reason=" << static_cast<unsigned>(candidate.decision.reason)
            << " generation_time_its_us="
            << candidate.input.generationTimeIts.count()

        << " reference_latitude_deg="
        << (candidate.input.referencePosition.latitudeDeg.has_value()
                ? std::to_string(
                      *candidate.input.referencePosition.latitudeDeg)
                : std::string("unavailable"))
        << " reference_longitude_deg="
        << (candidate.input.referencePosition.longitudeDeg.has_value()
                ? std::to_string(
                      *candidate.input.referencePosition.longitudeDeg)
                : std::string("unavailable"))
        << " role_on="
            << (candidate.input.vruRole.role ==
                        vanetza_integration::vru::role::VruRoleStatus::On
                    ? "true" : "false")
            << " vbs_active_standalone="
            << (candidate.input.vruRole.vbs ==
                        vanetza_integration::vru::role::VbsState::ActiveStandalone
                    ? "true" : "false")
            << " mode=EVALUATION_ONLY");

        // Evaluation is not transmission. Abort rather than confirm:
        // no successful-generation baseline may be committed here.
        if (!candidate.transactionHandle.has_value())
        {
            NS_LOG_UNCOND(
                "[VAM_GENERATION] CANDIDATE_HANDLE_MISSING"
                << " person_id=" << candidate.egoPersonId
                << " transaction_id=" << transactionId
                << " mode=EVALUATION_ONLY");
            continue;
        }

        // Encode the exact production candidate using the LF decision
        // made by its generation transaction. UPER bytes are evaluation
        // evidence only: they are not signed, transported or transmitted.
        VamUperEncodeOptions encodeOptions{};
        encodeOptions.includeLowFrequencyContainer =
            candidate.decision.includeLowFrequencyContainer;

        const VamUperEncodeResult encoded =
            VamUperEncoder::Encode(candidate.input, encodeOptions);

        if (encoded.success &&
            !encoded.bytes.empty() &&
            encoded.encodedBits > 0)
        {
            NS_LOG_UNCOND(
                "[VAM_GENERATION] CANDIDATE_ENCODED"
                << " person_id=" << candidate.egoPersonId
                << " station_id=" << candidate.stationId.value
                << " transaction_id=" << transactionId
                << " include_low_frequency_container="
                << (encodeOptions.includeLowFrequencyContainer
                        ? "true" : "false")
                << " encoded_bits=" << encoded.encodedBits
                << " encoded_bytes=" << encoded.bytes.size()
                << " mode=EVALUATION_ONLY");
        }
        else
        {
            NS_LOG_UNCOND(
                "[VAM_GENERATION] CANDIDATE_ENCODE_FAILED"
                << " person_id=" << candidate.egoPersonId
                << " station_id=" << candidate.stationId.value
                << " transaction_id=" << transactionId
                << " include_low_frequency_container="
                << (encodeOptions.includeLowFrequencyContainer
                        ? "true" : "false")
                << " reason="
                << (encoded.error.empty()
                        ? "encoder returned an empty or invalid result"
                        : encoded.error)
                << " mode=EVALUATION_ONLY");
        }

        // Encoding success is not transmission confirmation. Always abort
        // this evaluation-only transaction so no generation baseline commits.
        std::string abortError;
        if (!AbortVamGeneration(
                *candidate.transactionHandle,
                abortError))
        {
            NS_LOG_UNCOND(
                "[VAM_GENERATION] CANDIDATE_ABORT_FAILED"
                << " person_id=" << candidate.egoPersonId
                << " station_id=" << candidate.stationId.value
                << " transaction_id=" << transactionId
                << " error=" << abortError
                << " mode=EVALUATION_ONLY");
            continue;
        }

        NS_LOG_UNCOND(
            "[VAM_GENERATION] CANDIDATE_ABORTED"
            << " person_id=" << candidate.egoPersonId
            << " station_id=" << candidate.stationId.value
            << " transaction_id=" << transactionId
            << " baseline=UNCHANGED"
            << " mode=EVALUATION_ONLY");
    }

    // This stage deliberately stops at candidate evaluation.
    // Encoding, signing, BTP/GeoNetworking and transmission are not invoked.
}

void
SimulationController::HandleSumoStepComplete(std::uint64_t referenceTimeUs)
{
    RunCpmPerception(referenceTimeUs);
    RunVamGeneration(referenceTimeUs);
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

    DenmScenarioEvent resolvedEvent = event;
    std::optional<Wgs84Position> suppliedGeodetic =
        event.geodeticPosition;

    // The bridge publishes the vehicle state immediately before its DENM
    // event. Reuse it only when that projected sample matches the event.
    if (!suppliedGeodetic.has_value())
    {
        VehicleState* vehicle =
            VehicleRegistry::Instance().GetByVehId(event.vehicleId);

        if (vehicle != nullptr &&
            std::abs(vehicle->mobility.position.x - event.x) <= 0.002 &&
            std::abs(vehicle->mobility.position.y - event.y) <= 0.002 &&
            vehicle->mobility.geodeticPosition.has_value())
        {
            const auto& sample = *vehicle->mobility.geodeticPosition;
            suppliedGeodetic = Wgs84Position{
                sample.latitudeDeg,
                sample.longitudeDeg
            };
        }
    }

    Wgs84Position resolvedGeodetic{};
    std::string geodeticError;
    if (!SumoGeodeticTransform::ResolveWgs84(
            suppliedGeodetic,
            SimulationContext::sumoProjection,
            SumoProjectedPosition{event.x, event.y},
            resolvedGeodetic,
            geodeticError))
    {
        std::cout
            << "[DENM_EVENT_REJECTED]"
            << " vehicle=" << event.vehicleId
            << " event=" << event.eventId
            << " reason=INVALID_GEODETIC_POSITION"
            << " detail=" << geodeticError
            << '\n';
        return;
    }

    resolvedEvent.geodeticPosition = resolvedGeodetic;
    if (!m_denmScenarioEvents.AddEvent(resolvedEvent))
    {
        std::cout
            << "[DENM_EVENT_REJECTED]"
            << " vehicle=" << event.vehicleId
            << " event=" << event.eventId
            << " reason=EVENT_SOURCE_REJECTED"
            << '\n';
    }
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
