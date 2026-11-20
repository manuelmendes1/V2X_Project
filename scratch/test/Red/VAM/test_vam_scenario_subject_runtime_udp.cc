#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"
#include "vanetza_integration/sumo_adapter/sumo_network_projection_loader.h"
#include "vanetza_integration/context/its_time/its_start_time_parser.h"
#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"
#include "vanetza_integration/vam/codec/vam_uper_encoder.h"

#include <vanetza/common/clock.hpp>
#include <vanetza/asn1/vam.hpp>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/TrafficParticipantType.h>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/socket.h"
#include "ns3/wifi-module.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <utility>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace ns3;
using namespace vanetza_integration::vam;
using namespace vanetza_integration::vru::role;

namespace
{

constexpr std::uint32_t kScenarioStationId = 62101;
constexpr std::uint32_t kAtomicGoodStationId = 62001;
constexpr std::uint32_t kAtomicConflictStationId = 62002;

struct AbortFaultInjectionState
{
    bool enabled{false};
    std::size_t attempts{0};
};

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << label << '\n';
    return condition;
}

struct RuntimeCleanup
{
    int senderFd{-1};
    bool adapterInitialized{false};

    ~RuntimeCleanup()
    {
        if (senderFd >= 0)
        {
            close(senderFd);
        }

        if (!SimulationController::ShutdownVamGeneration())
        {
            std::cerr
                << "[VAM_SCENARIO_UDP] shutdown failed; skipping adapter "
                   "close and ITS identity clear\n";
            return;
        }

        if (adapterInitialized)
        {
            SumoAdapter::Close();
        }

        ItsStationIdentityAuthority::Instance().Clear();
    }
};

int FindAvailableUdpPort()
{
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(
            fd,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) != 0)
    {
        close(fd);
        return -1;
    }

    socklen_t addressLength = sizeof(address);
    if (getsockname(
            fd,
            reinterpret_cast<sockaddr*>(&address),
            &addressLength) != 0)
    {
        close(fd);
        return -1;
    }

    const int port = ntohs(address.sin_port);
    close(fd);
    return port;
}

bool SendDatagram(
    int senderFd,
    int destinationPort,
    const std::string& datagram)
{
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(
        static_cast<std::uint16_t>(destinationPort));

    if (inet_pton(
            AF_INET,
            "127.0.0.1",
            &destination.sin_addr) != 1)
    {
        return false;
    }

    const auto sent = sendto(
        senderFd,
        datagram.data(),
        datagram.size(),
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));

    if (sent != static_cast<ssize_t>(datagram.size()))
    {
        return false;
    }

    // Poll the real non-blocking adapter socket. The short retries only
    // allow the loopback datagram to become visible to recvfrom().
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        SumoAdapter::Poll();
    }

    return true;
}

VamGenerationConfig GenerationConfig()
{
    VamGenerationConfig config{};
    config.tCheckVamGenMs = 100;
    config.tGenVamMinMs = 100;
    config.tGenVamMaxMs = 5000;
    config.tGenVamLfMinMs = 2000;
    return config;
}

VamGenerationEvaluationAdapterConfig EvaluationConfig()
{
    VamGenerationEvaluationAdapterConfig config{};
    config.snapshotBuilder.vehicleThresholds = {2.0, 2.0, 1.0};
    config.snapshotBuilder.vruThresholds = {2.0, 2.0, 1.0};
    config.proximity.maximumSampleAgeUs = 250000;
    return config;
}

bool ConfigureActualScenarioSubject()
{
    VamVruScenarioSubjectConfig subject{};
    subject.personId = "runtime-vru";
    subject.stationId = kScenarioStationId;
    subject.activateRole = true;
    subject.vbsState = VbsState::ActiveStandalone;

    std::string error;
    return SimulationController::ConfigureVamVruScenarioSubjects(
        std::vector<VamVruScenarioSubjectConfig>{subject},
        error);
}


bool LoadRuntimeProjectionFixture(
    ns3::SumoUtmWgs84ProjectionConfig& output)
{
    constexpr const char* fixturePath =
        "vam-sumo-udp-projection.net.xml";

    std::ofstream fixture(
        fixturePath, std::ios::out | std::ios::trunc);
    if (!fixture) {
        std::cerr << "[FAIL] cannot create temporary SUMO projection fixture\n";
        return false;
    }

    fixture << R"(<?xml version="1.0" encoding="UTF-8"?>
<net>
  <location
    netOffset="-526913.640,-4576484.190"
    convBoundary="0.00,0.00,50000.00,50000.00"
    origBoundary="-8.4340762,41.4252900,-8.4000000,41.4500000"
    projParameter="+proj=utm +zone=29 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"/>
</net>
)";
    fixture.close();

    if (!fixture) {
        std::cerr << "[FAIL] cannot write temporary SUMO projection fixture\n";
        return false;
    }

    std::string error;
    if (!ns3::SumoNetworkProjectionLoader::LoadUtmWgs84(
            fixturePath, output, error)) {
        std::cerr << "[FAIL] SUMO projection fixture rejected: "
                  << error << '\n';
        return false;
    }

    return true;
}

bool EncodeUdpBackedCandidate(
    const VamGenerationConfig& generation,
    const VamGenerationEvaluationAdapterConfig& evaluation,
    const ns3::SumoUtmWgs84ProjectionConfig& projection,
    std::uint64_t referenceTimeUs)
{
    vanetza::Clock::time_point itsStartTime{};
    std::string error;

    if (!ItsStartTimeParser::ParseUtc(
            SimulationContext::itsStartTime,
            itsStartTime,
            error))
    {
        std::cerr << "[FAIL] observer ITS start time: "
                  << error << '\n';
        return false;
    }

    VamGenerationSchedulerConfig schedulerConfig{};
    schedulerConfig.itsStartClockUs =
        itsStartTime.time_since_epoch().count();
    schedulerConfig.generation = generation;
    schedulerConfig.evaluation = evaluation;
    schedulerConfig.sumoProjection = projection;

    VamGenerationScheduler observer(std::move(schedulerConfig));

    if (!observer.RegisterSubjectStationIdentity(
            "runtime-vru",
            VamStationId{kScenarioStationId},
            error))
    {
        std::cerr << "[FAIL] observer identity registration: "
                  << error << '\n';
        return false;
    }

    VamGenerationContext context{};
    std::vector<VamGenerationScheduledCandidate> candidates;
    VamGenerationSchedulerReport report{};

    // Consume the registries populated by the real UDP adapter.
    if (!observer.EvaluateStep(
            referenceTimeUs,
            SimulationController::GetSumoPersonRegistry(),
            SimulationController::GetSumoVruSubjectRegistry(),
            VehicleRegistry::Instance(),
            SumoVehicleLifecycleRegistry::Instance(),
            context,
            candidates,
            report,
            error))
    {
        std::cerr << "[FAIL] observer EvaluateStep: "
                  << error << '\n';
        return false;
    }

    if (candidates.size() != 1)
    {
        std::cerr << "[FAIL] expected exactly one UDP-backed candidate; got "
                  << candidates.size() << '\n';

        for (const auto& item : candidates)
        {
            if (item.transactionHandle.has_value())
            {
                std::string abortError;
                (void)observer.AbortGeneration(
                    *item.transactionHandle, abortError);
            }
        }

        return false;
    }

    const auto& candidate = candidates.front();

    bool passed = Check(
        candidate.egoPersonId == "runtime-vru" &&
            candidate.stationId.value == kScenarioStationId &&
            candidate.input.originatingStation.stationId.value ==
                kScenarioStationId &&
            candidate.input.vruProfile == VruProfile::Pedestrian &&
            candidate.input.referencePosition.latitudeDeg.has_value() &&
            candidate.input.referencePosition.longitudeDeg.has_value() &&
            candidate.transactionHandle.has_value(),
        "UDP_BACKED_VAM_CANDIDATE_REBUILT");

    // Encode the exact VamInput returned by the UDP-backed scheduler.
    const auto encoded = VamUperEncoder::Encode(candidate.input);

    const bool encodedOk =
        encoded.success &&
        !encoded.bytes.empty() &&
        encoded.encodedBits > 0 &&
        encoded.encodedBits <= encoded.bytes.size() * 8U &&
        encoded.encodedBits > (encoded.bytes.size() - 1U) * 8U;

    passed = Check(
        encodedOk,
        "UDP_BACKED_VAM_PROFILE1_UPER_ENCODED") && passed;

    // Decode using the same Vanetza R2 ASN.1 type as the encoder contract test.
    vanetza::asn1::r2::Vam decoded;

    const bool decodedOk =
        encodedOk &&
        decoded.decode(encoded.bytes.data(), encoded.bytes.size());

    passed = Check(
        decodedOk,
        "UDP_BACKED_VAM_R2_UPER_ROUNDTRIP_DECODED") && passed;

    const auto* root = decoded.operator->();

    const bool headerPreserved =
        root != nullptr &&
        root->header.protocolVersion == 3 &&
        root->header.messageId == Vanetza_ITS2_MessageId_vam &&
        root->header.stationId ==
            static_cast<long>(candidate.stationId.value);

    passed = Check(
        headerPreserved,
        "UDP_BACKED_VAM_HEADER_STATION_ID_PRESERVED") && passed;

    // GenerationDeltaTime represents the reference-position timestamp.
    const auto referencePositionTimeMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            candidate.input.referencePositionTimeIts).count();

    const bool referenceTimePreserved =
        root != nullptr &&
        referencePositionTimeMs >= 0 &&
        root->vam.generationDeltaTime ==
            static_cast<long>(referencePositionTimeMs % 65536);

    passed = Check(
        referenceTimePreserved,
        "UDP_BACKED_VAM_REFERENCE_TIME_PRESERVED") && passed;

    long long expectedLatitudeE7 = 0;
    long long expectedLongitudeE7 = 0;

    const bool hasGeodeticPosition =
        candidate.input.referencePosition.latitudeDeg.has_value() &&
        candidate.input.referencePosition.longitudeDeg.has_value();

    if (hasGeodeticPosition)
    {
        expectedLatitudeE7 = std::llround(
            *candidate.input.referencePosition.latitudeDeg * 10000000.0);

        expectedLongitudeE7 = std::llround(
            *candidate.input.referencePosition.longitudeDeg * 10000000.0);
    }

    const bool geodeticPositionPreserved =
        root != nullptr &&
        hasGeodeticPosition &&
        root->vam.vamParameters.basicContainer.referencePosition.latitude ==
            expectedLatitudeE7 &&
        root->vam.vamParameters.basicContainer.referencePosition.longitude ==
            expectedLongitudeE7;

    passed = Check(
        geodeticPositionPreserved,
        "UDP_BACKED_VAM_WGS84_POSITION_PRESERVED") && passed;

    const bool pedestrianTypePreserved =
        root != nullptr &&
        root->vam.vamParameters.basicContainer.stationType ==
            Vanetza_ITS2_TrafficParticipantType_pedestrian;

    passed = Check(
        pedestrianTypePreserved,
        "UDP_BACKED_VAM_PEDESTRIAN_TYPE_PRESERVED") && passed;

    // Resolve the observer's own transaction. The production controller
    // continues to manage and abort its independent evaluation transaction.
    bool observerAborted = false;

    if (candidate.transactionHandle.has_value())
    {
        std::string abortError;

        observerAborted =
            observer.AbortGeneration(
                *candidate.transactionHandle, abortError) &&
            observer.GetPendingCandidates().empty();

        if (!observerAborted)
        {
            std::cerr << "[FAIL] observer transaction abort: "
                      << abortError << '\n';
        }
    }

    passed = Check(
        observerAborted,
        "UDP_BACKED_VAM_OBSERVER_TRANSACTION_ABORTED") && passed;

    if (encodedOk &&
        decodedOk &&
        headerPreserved &&
        referenceTimePreserved &&
        geodeticPositionPreserved &&
        pedestrianTypePreserved &&
        observerAborted)
    {
        const auto& position =
            root->vam.vamParameters.basicContainer.referencePosition;

        std::cout
            << "[VAM_UPER_INTEGRATION] ROUNDTRIP_OK"
            << " candidate_source=UDP_BACKED"
            << " person_id=" << candidate.egoPersonId
            << " station_id=" << candidate.stationId.value
            << " generation_delta_time_ms="
            << root->vam.generationDeltaTime
            << " latitude_e7=" << position.latitude
            << " longitude_e7=" << position.longitude
            << " station_type=pedestrian"
            << " encoded_bits=" << encoded.encodedBits
            << " mode=EVALUATION_ONLY"
            << '\n';
    }

    return passed;
}

bool RunIntegration()
{
    RuntimeCleanup cleanup;

    auto& authority = ItsStationIdentityAuthority::Instance();
    authority.Clear();

    VehicleRegistry::Instance().Clear();
    SumoVehicleLifecycleRegistry::Instance().Clear();

    // This test is a fresh process. Explicitly clear any scenario declaration
    // before exercising the public configuration API.
    SimulationController::ShutdownVamGeneration();

    std::string error;
    const std::vector<VamVruScenarioSubjectConfig> noSubjects;
    if (!Check(
            SimulationController::ConfigureVamVruScenarioSubjects(
                noSubjects, error),
            "EMPTY_SCENARIO_CONFIGURATION_RESET"))
    {
        return false;
    }

    SimulationContext::itsStartTime = "2026-10-22T10:00:00";

    const auto generation = GenerationConfig();
    const auto evaluation = EvaluationConfig();

    // The same shared executor is passed to the initial scheduler and its
    // valid replacement. Fault injection is armed only for the final lifecycle
    // scenario; normal earlier candidates use the production abort operation.
    const auto abortFaultState = std::make_shared<AbortFaultInjectionState>();
    VamGenerationAbortExecutor abortExecutor =
        [abortFaultState](
            VamGenerationManager& manager,
            std::uint64_t transactionId,
            std::string& abortError)
        {
            if (!abortFaultState->enabled)
            {
                return manager.AbortGeneration(transactionId, abortError);
            }

            ++abortFaultState->attempts;
            if (abortFaultState->attempts <= 3)
            {
                abortError = "deterministic controller lifecycle abort fault #" +
                    std::to_string(abortFaultState->attempts);
                return false;
            }

            return manager.AbortGeneration(transactionId, abortError);
        };

    ns3::SumoUtmWgs84ProjectionConfig sumoProjection{};
    if (!Check(
            LoadRuntimeProjectionFixture(sumoProjection),
            "SUMO_NETWORK_PROJECTION_LOADED_BY_RUNTIME_HARNESS")) {
        return false;
    }

    YansWifiChannelHelper channelHelper;
    channelHelper.SetPropagationDelay(
        "ns3::ConstantSpeedPropagationDelayModel");
    channelHelper.AddPropagationLoss(
        "ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(2.7),
        "ReferenceDistance", DoubleValue(1.0));
    Ptr<YansWifiChannel> endpointChannel = channelHelper.Create();
    Ipv4AddressHelper endpointIpv4;
    endpointIpv4.SetBase("10.93.0.0", "255.255.255.0");
    Callback<void, Ptr<Socket>> noReceiveCallback;
    VehicleFactory::Initialize(
        endpointChannel, endpointIpv4, noReceiveCallback);

    if (!Check(
            endpointChannel != nullptr,
            "VAM_ENDPOINT_WIFI_CHANNEL_INITIALIZED")) {
        return false;
    }

    if (!Check(
            SimulationController::ConfigureVamGeneration(
                true, generation, evaluation, error, sumoProjection, abortExecutor),
            "INITIAL_VAM_SCHEDULER_CONFIGURED")) {
        std::cerr << "Configuration error: " << error << '\n';
        return false;
    }

    // Every invalid replacement must be rejected before publication.
    // The original scheduler remains active and is exercised below by the
    // real SUMO_STEP_END callback and candidate evaluation.
    const auto rejectsInvalidGeneration =
        [&](const VamGenerationConfig& invalid, const char* marker)
        {
            error.clear();
            return Check(
                !SimulationController::ConfigureVamGeneration(
                    true, invalid, evaluation, error) &&
                    !error.empty(),
                marker);
        };

    auto invalidGeneration = generation;

    invalidGeneration.tCheckVamGenMs = 0;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_ZERO_CHECK_INTERVAL_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.tCheckVamGenMs =
        invalidGeneration.tGenVamMinMs + 1;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_CHECK_INTERVAL_ABOVE_MIN_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.tGenVamMinMs = 0;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_ZERO_MIN_INTERVAL_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.tGenVamMaxMs =
        invalidGeneration.tGenVamMinMs - 1;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_MAX_GENERATION_INTERVAL_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.tGenVamLfMinMs = 0;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_LOW_FREQUENCY_INTERVAL_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.numSkipVamsForRedundancyMitigation = 1;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_SKIP_COUNT_BELOW_RANGE_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.numSkipVamsForRedundancyMitigation = 11;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_SKIP_COUNT_ABOVE_RANGE_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.minPositionChangeThresholdMeters = 0.0;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_POSITION_CHANGE_THRESHOLD_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.minGroundSpeedChangeThresholdMps =
        std::numeric_limits<double>::quiet_NaN();
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_GROUND_SPEED_THRESHOLD_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration.minGroundVelocityOrientationChangeThresholdDegrees =
        181.0;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_ORIENTATION_THRESHOLD_REJECTED"))
    {
        return false;
    }

    invalidGeneration = generation;
    invalidGeneration
        .trajectoryInterceptionProbabilityChangeThresholdPercentagePoints =
        101.0;
    if (!rejectsInvalidGeneration(
            invalidGeneration, "INVALID_INTERCEPTION_THRESHOLD_REJECTED"))
    {
        return false;
    }

    auto invalidEvaluation = evaluation;
    invalidEvaluation.snapshotBuilder.vehicleThresholds
        .minimumSafeLateralDistanceMeters = 0.0;
    error.clear();

    if (!Check(
            !SimulationController::ConfigureVamGeneration(
                true, generation, invalidEvaluation, error) &&
                !error.empty(),
            "INVALID_PROXIMITY_THRESHOLD_REJECTED"))
    {
        return false;
    }

    // Exercise the atomic reservation rollback. The staged map is ordered
    // lexicographically, so atomic-a-good is reserved before the conflict.
    const ItsStationIdentityOwner collisionOwner{
        ItsStationIdentityOwnerKind::Vehicle,
        "harness-collision-owner"
    };

    if (!Check(
            authority.Reserve(
                collisionOwner, kAtomicConflictStationId),
            "ATOMIC_CONFLICT_FIXTURE_RESERVED"))
    {
        return false;
    }

    const std::vector<VamVruScenarioSubjectConfig> conflictingSubjects{
        {
            "atomic-a-good",
            kAtomicGoodStationId,
            false,
            VbsState::Idle
        },
        {
            "atomic-z-conflict",
            kAtomicConflictStationId,
            false,
            VbsState::Idle
        }
    };

    error.clear();
    const bool conflictingConfigurationAccepted =
        SimulationController::ConfigureVamVruScenarioSubjects(
            conflictingSubjects, error);

    const ItsStationIdentityOwner rolledBackOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:atomic-a-good"
    };

    if (!Check(
            !conflictingConfigurationAccepted && !error.empty(),
            "ATOMIC_SCENARIO_ID_CONFLICT_REJECTED"))
    {
        return false;
    }

    if (!Check(
            !authority.FindStationId(rolledBackOwner).has_value(),
            "ATOMIC_ROLLBACK_RELEASED_NEW_RESERVATION"))
    {
        return false;
    }

    if (!Check(
            authority.IsReservedBy(
                kAtomicConflictStationId, collisionOwner),
            "ATOMIC_ROLLBACK_PRESERVED_EXISTING_OWNER"))
    {
        return false;
    }

    if (!Check(
            authority.Release(
                collisionOwner, kAtomicConflictStationId),
            "ATOMIC_CONFLICT_FIXTURE_RELEASED"))
    {
        return false;
    }

    error.clear();
    if (!Check(
            ConfigureActualScenarioSubject(),
            "SCENARIO_SUBJECT_CONFIGURATION_ACCEPTED"))
    {
        std::cerr << "Scenario configuration error: " << error << '\n';
        return false;
    }

    const ItsStationIdentityOwner scenarioOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:runtime-vru"
    };

    const auto reservedStationId =
        authority.FindStationId(scenarioOwner);

    if (!Check(
            reservedStationId.has_value() &&
                *reservedStationId == kScenarioStationId,
            "SCENARIO_SUBJECT_HAS_EXPLICIT_GLOBAL_STATION_ID"))
    {
        return false;
    }

    // Install the production callbacks and initialize the real UDP adapter.
    SimulationController::ConfigureSumoAdapterCallbacks();

    const int localPort = FindAvailableUdpPort();
    if (!Check(localPort > 0, "UDP_LOOPBACK_PORT_ALLOCATED"))
    {
        return false;
    }

    if (!Check(
            SumoAdapter::Init(localPort, "127.0.0.1", 7000),
            "REAL_SUMO_ADAPTER_INITIALIZED"))
    {
        return false;
    }
    cleanup.adapterInitialized = true;

    cleanup.senderFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!Check(
            cleanup.senderFd >= 0,
            "UDP_INJECTOR_SOCKET_CREATED"))
    {
        return false;
    }

    std::cout << "VAM_SCENARIO_UDP_LOCAL_PORT=" << localPort << '\n';

    // First presence: the real SUMO_PERSON callback synchronizes the
    // person/subject registries and applies the scenario-owned role.
    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_PERSON,UPSERT,runtime-vru,20374.270,9640.360,1,90,1000,pedestrian"),
            "UDP_INITIAL_PERSON_UPSERT_SENT"))
    {
        return false;
    }

    const auto firstPerson =
        SimulationController::GetSumoPersonRegistry().Find("runtime-vru");
    const auto firstSubject =
        SimulationController::GetSumoVruSubjectRegistry().Find("runtime-vru");

    if (!Check(
            firstPerson.has_value() &&
                firstSubject.has_value() &&
                firstSubject->profile == VruProfile::Pedestrian &&
                firstSubject->roleState.role == VruRoleStatus::On &&
                firstSubject->roleState.vbs == VbsState::ActiveStandalone,
            "UDP_UPSERT_APPLIED_SCENARIO_PROFILE_AND_ROLE"))
    {
        return false;
    }

    // Test-local observer: rebuild from the UDP-populated registries and
    // encode/decode the exact scheduler-produced VamInput.
    if (!EncodeUdpBackedCandidate(
            generation, evaluation, sumoProjection, 100000))
    {
        return false;
    }

    // The step-end datagram must reach the controller's registered callback
    // and produce an evaluation candidate using the still-active scheduler.
    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_STEP_END,100000"),
            "UDP_STEP_END_SENT"))
    {
        return false;
    }

    if (!Check(
            SimulationController::GetPendingVamGenerationCandidates().empty(),
            "FIRST_EVALUATION_ONLY_CANDIDATE_ABORTED"))
    {
        return false;
    }

    // An aborted evaluation must leave the manager eligible to produce
    // another InitialActivation candidate at the next T_CheckVamGen.
    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_STEP_END,200000"),
            "UDP_RETRY_STEP_END_SENT"))
    {
        return false;
    }

    if (!Check(
            SimulationController::GetPendingVamGenerationCandidates().empty(),
            "SECOND_EVALUATION_ONLY_CANDIDATE_ABORTED"))
    {
        return false;
    }

    // A manual deactivation must survive a later UPSERT in the same
    // presence period. Scenario settings are applied only on first presence.
    const auto deactivated =
        SimulationController::DeactivateVruSubject("runtime-vru");

    if (!Check(
            deactivated.has_value() &&
                *deactivated == VruTransitionResult::Applied,
            "EXPLICIT_ROLE_DEACTIVATION_APPLIED"))
    {
        return false;
    }

    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_PERSON,UPSERT,runtime-vru,20375.270,9640.360,1,90,1500,pedestrian"),
            "UDP_REPEATED_PERSON_UPSERT_SENT"))
    {
        return false;
    }

    const auto repeatedSubject =
        SimulationController::GetSumoVruSubjectRegistry().Find("runtime-vru");

    if (!Check(
            repeatedSubject.has_value() &&
                repeatedSubject->roleState.role == VruRoleStatus::Off &&
                repeatedSubject->roleState.vbs == VbsState::Idle,
            "REPEATED_UPSERT_DID_NOT_REACTIVATE_DEACTIVATED_SUBJECT"))
    {
        return false;
    }

    // A valid scheduler replacement must retain both the scenario declaration
    // and the global ITS identity reservation.
    error.clear();
    if (!Check(
            SimulationController::ConfigureVamGeneration(true, generation, evaluation, error, sumoProjection, abortExecutor),
            "VALID_SCHEDULER_RECONFIGURATION_ACCEPTED"))
    {
        std::cerr << "Reconfiguration error: " << error << '\n';
        return false;
    }

    // The preceding regression deliberately deactivated this subject and
    // proved that a repeated UPSERT does not reactivate it. Restore the
    // explicit role/VBS policy here so the fault-injection phase can produce
    // a real candidate. This is an explicit state transition, not an
    // automatic side effect of SUMO UPSERT.
    const auto faultRoleActivation =
        SimulationController::ActivateVruSubject("runtime-vru");
    if (!Check(
            faultRoleActivation.has_value() &&
                *faultRoleActivation == VruTransitionResult::Applied,
            "FAULT_FIXTURE_ROLE_REACTIVATED"))
    {
        return false;
    }

    const auto faultVbsActivation =
        SimulationController::SetVruSubjectVbsState(
            "runtime-vru", VbsState::ActiveStandalone);
    if (!Check(
            faultVbsActivation.has_value() &&
                *faultVbsActivation == VruTransitionResult::Applied,
            "FAULT_FIXTURE_VBS_REACTIVATED"))
    {
        return false;
    }

    // VAM_ABORT_FAULT_FIXTURE_ROLE_REACTIVATION_V3
    // Arm deterministic failures only after explicit policy activation.
    abortFaultState->enabled = true;
    if (!Check(
            abortFaultState->attempts == 0,
            "ABORT_FAULT_INJECTION_ARMED"))
    {
        return false;
    }

    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_STEP_END,300000"),
            "UDP_OLD_PRESENCE_FAULT_STEP_END_SENT"))
    {
        return false;
    }

    const auto oldPresencePending =
        SimulationController::GetPendingVamGenerationCandidates();
    if (!Check(
            abortFaultState->attempts == 1 &&
                oldPresencePending.size() == 1 &&
                oldPresencePending.front().egoPersonId == "runtime-vru",
            "OLD_PRESENCE_CANDIDATE_ABORT_FAILED_AND_RETAINED"))
    {
        return false;
    }

    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_PERSON,REMOVE,runtime-vru,2000"),
            "UDP_PERSON_REMOVE_SENT"))
    {
        return false;
    }

    if (!Check(
            !SimulationController::GetSumoPersonRegistry()
                 .Find("runtime-vru").has_value() &&
                !SimulationController::GetSumoVruSubjectRegistry()
                     .Find("runtime-vru").has_value(),
            "REMOVE_CLEARED_PERSON_AND_VRU_SUBJECT"))
    {
        return false;
    }

    const auto identityAfterRemove =
        ItsStationIdentityAuthority::Instance().FindStationId(
            ItsStationIdentityOwner{
                ItsStationIdentityOwnerKind::ItsStation,
                "vam-vru:runtime-vru"});
    if (!Check(
            identityAfterRemove.has_value() &&
                *identityAfterRemove == kScenarioStationId,
            "REMOVE_PRESERVED_LOGICAL_STATION_RESERVATION"))
    {
        return false;
    }

    if (!Check(
            abortFaultState->attempts == 2,
            "REMOVE_RETAINED_OLD_PRESENCE_RUNTIME"))
    {
        return false;
    }

    // Re-entry uses a different presence epoch while the old retired runtime
    // and endpoint remain owned by the scheduler/controller.
    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_PERSON,UPSERT,runtime-vru,20376.270,9640.360,1,90,5000,pedestrian"),
            "UDP_PERSON_REENTRY_UPSERT_SENT"))
    {
        return false;
    }

    const auto reenteredPerson =
        SimulationController::GetSumoPersonRegistry().Find("runtime-vru");
    const auto reenteredSubject =
        SimulationController::GetSumoVruSubjectRegistry().Find("runtime-vru");

    if (!Check(
            reenteredPerson.has_value() &&
                reenteredPerson->firstObservedAtUs == 5000 &&
                reenteredSubject.has_value() &&
                reenteredSubject->profile == VruProfile::Pedestrian &&
                reenteredSubject->roleState.role == VruRoleStatus::On &&
                reenteredSubject->roleState.vbs ==
                    VbsState::ActiveStandalone,
            "REENTRY_REAPPLIED_SCENARIO_PROFILE_AND_ROLE"))
    {
        return false;
    }

    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_STEP_END,400000"),
            "UDP_RETIRED_RETRY_STEP_END_SENT"))
    {
        return false;
    }

    if (!Check(
            abortFaultState->attempts == 3 &&
                reenteredPerson.has_value() &&
                reenteredPerson->firstObservedAtUs == 5000,
            "RETIRED_RUNTIME_RETRY_STILL_QUARANTINED"))
    {
        return false;
    }

    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_STEP_END,500000"),
            "UDP_RETIRED_RECOVERY_STEP_END_SENT"))
    {
        return false;
    }

    if (!Check(
            abortFaultState->attempts == 5 &&
                SimulationController::GetPendingVamGenerationCandidates().empty(),
            "RETIRED_RUNTIME_RECOVERY_COMPLETED"))
    {
        return false;
    }

    std::cout << "VAM_ABORT_FAULT_INJECTION_ATTEMPTS="
              << abortFaultState->attempts << '\n';

    std::cout << "VAM_SCENARIO_UDP_RUNTIME=PASS\n";
    return true;
}

} // namespace

int main()
{
    const bool passed = RunIntegration();
    return passed ? 0 : 1;
}
