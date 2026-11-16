#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
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

        if (adapterInitialized)
        {
            SumoAdapter::Close();
        }

        SimulationController::ShutdownVamGeneration();
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

    if (!Check(
            SimulationController::ConfigureVamGeneration(
                true, generation, evaluation, error),
            "INITIAL_VAM_SCHEDULER_CONFIGURED"))
    {
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
                "SUMO_PERSON,UPSERT,runtime-vru,0,0,1,90,1000,pedestrian"),
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
                "SUMO_PERSON,UPSERT,runtime-vru,1,0,1,90,1500,pedestrian"),
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
            SimulationController::ConfigureVamGeneration(
                true, generation, evaluation, error),
            "VALID_SCHEDULER_RECONFIGURATION_ACCEPTED"))
    {
        std::cerr << "Reconfiguration error: " << error << '\n';
        return false;
    }

    // Remove the active presence. The subject and mobility sample disappear,
    // but the logical station identity remains reserved.
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
        authority.FindStationId(scenarioOwner);

    if (!Check(
            identityAfterRemove.has_value() &&
                *identityAfterRemove == kScenarioStationId,
            "REMOVE_PRESERVED_LOGICAL_STATION_RESERVATION"))
    {
        return false;
    }

    // Re-entry creates a new OFF/IDLE subject first; the scenario declaration
    // must then be reapplied and the scheduler must resolve the same stationId.
    if (!Check(
            SendDatagram(
                cleanup.senderFd,
                localPort,
                "SUMO_PERSON,UPSERT,runtime-vru,2,0,1,90,3000,pedestrian"),
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
                "SUMO_STEP_END,200000"),
            "UDP_REENTRY_STEP_END_SENT"))
    {
        return false;
    }

    if (!Check(
            SimulationController::GetPendingVamGenerationCandidates().empty(),
            "REENTRY_EVALUATION_ONLY_CANDIDATE_ABORTED"))
    {
        return false;
    }

    std::cout << "VAM_SCENARIO_UDP_RUNTIME=PASS\n";
    return true;
}

} // namespace

int main()
{
    const bool passed = RunIntegration();
    return passed ? 0 : 1;
}
