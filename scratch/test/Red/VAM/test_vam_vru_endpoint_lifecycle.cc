#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/vam/emitter/vam_vru_emitter_endpoint_store.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_network/vehicle_network_builder.h"
#include "vanetza_integration/vam/emitter/vam_vru_emitter_endpoint.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/vam/scheduler/vam_generation_scheduler.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/ipv4.h"
#include "ns3/node-list.h"
#include "ns3/simulator.h"
#include "ns3/wifi-module.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

using namespace ns3;

namespace
{

constexpr const char* kPersonId = "runtime-vru-lifecycle";
constexpr std::uint32_t kStationId = 63201;
constexpr double kTolerance = 1e-6;

bool Check(bool condition, const char* marker)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << marker << '\n';
    return condition;
}

bool NearlyEqual(double left, double right)
{
    return std::abs(left - right) <= kTolerance;
}

struct EndpointSnapshot
{
    Ptr<Node> node;
    std::uint32_t nodeId{};
    Ipv4Address ipv4;
    std::uint32_t stationId{};
    vanetza::geonet::StationType stationType =
        vanetza::geonet::StationType::Unknown;
    StationEndpointKind kind = StationEndpointKind::Vehicle;
};

bool FindVruEndpoint(
    const std::string& personId,
    EndpointSnapshot& output)
{
    const std::string logicalId = "vam-vru:" + personId;
    const auto& registry = StationEndpointRegistry::Instance();

    for (auto nodeIt = NodeList::Begin(); nodeIt != NodeList::End(); ++nodeIt)
    {
        Ptr<Node> node = *nodeIt;
        if (node == nullptr)
        {
            continue;
        }

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (ipv4 == nullptr)
        {
            continue;
        }

        for (std::uint32_t interfaceIndex = 0;
             interfaceIndex < ipv4->GetNInterfaces();
             ++interfaceIndex)
        {
            for (std::uint32_t addressIndex = 0;
                 addressIndex < ipv4->GetNAddresses(interfaceIndex);
                 ++addressIndex)
            {
                const Ipv4Address address =
                    ipv4->GetAddress(interfaceIndex, addressIndex).GetLocal();
                const StationEndpointIdentity* identity =
                    registry.GetByIpv4(address);

                if (identity == nullptr ||
                    identity->kind != StationEndpointKind::ItsStation ||
                    identity->logicalId != logicalId)
                {
                    continue;
                }

                output.node = node;
                output.nodeId = identity->nodeId;
                output.ipv4 = identity->ipv4Address;
                output.stationId = identity->stationId;
                output.stationType = identity->stationType;
                output.kind = identity->kind;
                return true;
            }
        }
    }

    return false;
}

bool CheckMobility(
    const EndpointSnapshot& endpoint,
    const Vector& expectedPosition,
    const Vector& expectedVelocity)
{
    if (endpoint.node == nullptr)
    {
        return false;
    }

    Ptr<ConstantVelocityMobilityModel> mobility =
        endpoint.node->GetObject<ConstantVelocityMobilityModel>();
    if (mobility == nullptr)
    {
        return false;
    }

    const Vector position = mobility->GetPosition();
    const Vector velocity = mobility->GetVelocity();
    return NearlyEqual(position.x, expectedPosition.x) &&
           NearlyEqual(position.y, expectedPosition.y) &&
           NearlyEqual(position.z, expectedPosition.z) &&
           NearlyEqual(velocity.x, expectedVelocity.x) &&
           NearlyEqual(velocity.y, expectedVelocity.y) &&
           NearlyEqual(velocity.z, expectedVelocity.z);
}

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

    if (bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
    {
        close(fd);
        return -1;
    }

    socklen_t length = sizeof(address);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) != 0)
    {
        close(fd);
        return -1;
    }

    const int port = ntohs(address.sin_port);
    close(fd);
    return port;
}

bool SendDatagram(int senderFd, int destinationPort, const std::string& text)
{
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(static_cast<std::uint16_t>(destinationPort));
    if (inet_pton(AF_INET, "127.0.0.1", &destination.sin_addr) != 1)
    {
        return false;
    }

    const ssize_t sent = sendto(
        senderFd,
        text.data(),
        text.size(),
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));
    if (sent != static_cast<ssize_t>(text.size()))
    {
        return false;
    }

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        SumoAdapter::Poll();
    }
    return true;
}

struct RuntimeCleanup
{
    int senderFd{-1};
    bool adapterInitialized{false};
    bool complete{false};

    bool Cleanup()
    {
        if (complete)
        {
            return true;
        }

        if (senderFd >= 0)
        {
            close(senderFd);
            senderFd = -1;
        }

        if (!SimulationController::ShutdownVamGeneration())
        {
            std::cerr
                << "[VAM_VRU_LIFECYCLE] shutdown blocked; preserving station "
                   "registry and ITS identity authority\n";
            return false;
        }

        if (adapterInitialized)
        {
            SumoAdapter::Close();
            adapterInitialized = false;
        }

        StationEndpointRegistry::Instance().Clear();
        Simulator::Destroy();
        ItsStationIdentityAuthority::Instance().Clear();
        VehicleRegistry::Instance().Clear();
        complete = true;
        return true;
    }

    ~RuntimeCleanup()
    {
        (void)Cleanup();
    }
};

bool RunConcurrentPresenceEndpointOwnership()
{
    constexpr const char* personId = "endpoint-presence-coexistence-probe";
    constexpr std::uint32_t stationId = 63202;

    VamVruEmitterConfig oldConfig{};
    oldConfig.personId = personId;
    oldConfig.stationId = stationId;
    oldConfig.presenceEpoch = {1000};
    oldConfig.initialPosition = Vector(10.0, 20.0, 0.0);
    oldConfig.initialVelocity = Vector(1.0, 0.0, 0.0);

    auto oldEndpoint = VamVruEmitterEndpoint::Create(oldConfig);
    if (!Check(oldEndpoint != nullptr, "OLD_PRESENCE_ENDPOINT_CREATED"))
    {
        return false;
    }

    VamVruEmitterConfig newConfig = oldConfig;
    newConfig.presenceEpoch = {2000};
    newConfig.initialPosition = Vector(30.0, 40.0, 0.0);
    newConfig.initialVelocity = Vector(0.0, 2.0, 0.0);

    auto newEndpoint = VamVruEmitterEndpoint::Create(newConfig);
    if (!Check(newEndpoint != nullptr, "NEW_PRESENCE_ENDPOINT_CREATED"))
    {
        return false;
    }

    bool passed = true;
    passed = Check(
        oldEndpoint->PresenceEpoch().value == 1000 &&
            newEndpoint->PresenceEpoch().value == 2000 &&
            oldEndpoint->PersonId() == newEndpoint->PersonId() &&
            oldEndpoint->StationId() == newEndpoint->StationId(),
        "CONCURRENT_PRESENCE_ENDPOINTS_CREATED_WITH_DISTINCT_EPOCHS") && passed;

    const bool oldInitialized =
        VehicleNetworkBuilder::InitializeVamVruEmitter(*oldEndpoint);
    const bool newInitialized =
        VehicleNetworkBuilder::InitializeVamVruEmitter(*newEndpoint);
    if (!Check(
            oldInitialized && newInitialized,
            "CONCURRENT_PRESENCE_ENDPOINT_NETWORKS_INITIALIZED"))
    {
        return false;
    }

    const Ipv4Address oldIpv4 = oldEndpoint->GetIpv4Address();
    const Ipv4Address newIpv4 = newEndpoint->GetIpv4Address();
    const auto& registry = StationEndpointRegistry::Instance();
    const StationEndpointIdentity* oldEntry = registry.GetByIpv4(oldIpv4);
    const StationEndpointIdentity* newEntry = registry.GetByIpv4(newIpv4);

    const bool bothRegistered =
        oldIpv4 != newIpv4 &&
        oldEntry != nullptr &&
        newEntry != nullptr &&
        oldEntry->logicalId == std::string("vam-vru:") + personId &&
        newEntry->logicalId == std::string("vam-vru:") + personId &&
        oldEntry->stationId == stationId &&
        newEntry->stationId == stationId &&
        oldEntry->stationType == vanetza::geonet::StationType::Pedestrian &&
        newEntry->stationType == vanetza::geonet::StationType::Pedestrian;
    passed = Check(
        bothRegistered,
        "COEXISTING_PRESENCE_ENDPOINTS_HAVE_DISTINCT_IPV4_REGISTRATIONS") && passed;

    oldEndpoint.reset();
    oldEntry = registry.GetByIpv4(oldIpv4);
    newEntry = registry.GetByIpv4(newIpv4);
    passed = Check(
        oldEntry == nullptr &&
            newEntry != nullptr &&
            newEntry->logicalId == std::string("vam-vru:") + personId &&
            newEntry->stationId == stationId,
        "DESTROYING_OLD_ENDPOINT_PRESERVES_NEW_REGISTRY_ENTRY") && passed;

    newEndpoint.reset();
    const ItsStationIdentityOwner owner{
        ItsStationIdentityOwnerKind::ItsStation,
        std::string("vam-vru:") + personId
    };
    passed = Check(
        registry.GetByIpv4(newIpv4) == nullptr &&
            ItsStationIdentityAuthority::Instance().IsReservedBy(stationId, owner),
        "COEXISTENT_ENDPOINT_CLEANUP_PRESERVES_LOGICAL_STATION_IDENTITY") && passed;

    return passed;
}

bool RunVamVruEndpointOwnershipStoreLifecycle()
{
    auto& authority = ItsStationIdentityAuthority::Instance();
    authority.Clear();

    VamVruEmitterEndpointStore store;
    VamVruEmitterConfig oldConfig{};
    oldConfig.personId = "endpoint-store-vru";
    oldConfig.stationId = 63301;
    oldConfig.presenceEpoch = {1000};
    oldConfig.initialPosition = Vector(1.0, 2.0, 0.0);
    oldConfig.initialVelocity = Vector(0.0, 0.0, 0.0);

    auto oldEndpoint = VamVruEmitterEndpoint::Create(oldConfig);
    bool ok = Check(
        oldEndpoint != nullptr &&
            store.InsertActive(std::move(oldEndpoint)) &&
            store.ActiveCount() == 1 && store.RetiredCount() == 0,
        "OWNERSHIP_STORE_INSERTS_ACTIVE_ENDPOINT");

    ok &= Check(
        store.RetireActive(oldConfig.personId, true) &&
            store.ActiveCount() == 0 && store.RetiredCount() == 1 &&
            store.FindRetired(oldConfig.personId, {1000}) != nullptr,
        "OWNERSHIP_STORE_RETAINS_ENDPOINT_FOR_OWNED_PRESENCE");

    VamVruEmitterConfig newConfig = oldConfig;
    newConfig.presenceEpoch = {2000};
    auto newEndpoint = VamVruEmitterEndpoint::Create(newConfig);
    ok &= Check(
        newEndpoint != nullptr && store.InsertActive(std::move(newEndpoint)) &&
            store.ActiveCount() == 1 && store.RetiredCount() == 1 &&
            store.FindActive(newConfig.personId) != nullptr &&
            store.FindActive(newConfig.personId)->PresenceEpoch().value == 2000,
        "OWNERSHIP_STORE_ADMITS_NEW_PRESENCE_WITH_RETIRED_PREDECESSOR");

    ok &= Check(
        store.ReconcileActivePresence(newConfig.personId, {2000}, false) &&
            store.FindActive(newConfig.personId) != nullptr &&
            store.FindActive(newConfig.personId)->PresenceEpoch().value == 2000 &&
            store.RetiredCount() == 1,
        "OWNERSHIP_STORE_REPEATED_UPSERT_REUSES_ONLY_MATCHING_EPOCH");

    const auto retainedCount = store.PruneRetired(
        [](const std::string& personId,
           vanetza_integration::vam::VamPersonPresenceEpoch epoch)
        {
            return personId == "endpoint-store-vru" && epoch.value == 1000;
        });
    ok &= Check(
        retainedCount == 0 && store.RetiredCount() == 1 &&
            store.FindRetired(oldConfig.personId, {1000}) != nullptr,
        "OWNERSHIP_STORE_PRUNING_PRESERVES_STILL_OWNED_EPOCH");

    const auto releasedCount = store.PruneRetired(
        [](const std::string&,
           vanetza_integration::vam::VamPersonPresenceEpoch)
        {
            return false;
        });
    ok &= Check(
        releasedCount == 1 && store.RetiredCount() == 0 &&
            store.FindRetired(oldConfig.personId, {1000}) == nullptr &&
            store.FindActive(newConfig.personId) != nullptr,
        "OWNERSHIP_STORE_PRUNING_RELEASES_ONLY_RESOLVED_OLD_EPOCH");

    ok &= Check(
        store.RetireActive(newConfig.personId, false) &&
            store.ActiveCount() == 0 && store.RetiredCount() == 0,
        "OWNERSHIP_STORE_DESTROYS_ENDPOINT_WITHOUT_RETAINED_RUNTIME");

    const ItsStationIdentityOwner owner{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:endpoint-store-vru"
    };
    ok &= Check(
        authority.IsReservedBy(63301, owner),
        "OWNERSHIP_STORE_ENDPOINT_RETIREMENT_PRESERVES_ITS_IDENTITY");

    store.ClearAll();
    authority.Clear();
    std::cout << "VAM_VRU_ENDPOINT_OWNERSHIP_STORE="
              << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

bool RunLifecycleIntegration()
{
    RuntimeCleanup cleanup;
    auto& authority = ItsStationIdentityAuthority::Instance();
    authority.Clear();
    StationEndpointRegistry::Instance().Clear();
    VehicleRegistry::Instance().Clear();

    if (!Check(
            SimulationController::ShutdownVamGeneration(),
            "PRIOR_VAM_SCHEDULER_CLEANED"))
    {
        return false;
    }

    std::string error;
    const std::vector<VamVruScenarioSubjectConfig> noSubjects;
    if (!Check(
            SimulationController::ConfigureVamVruScenarioSubjects(noSubjects, error),
            "SCENARIO_DECLARATIONS_RESET"))
    {
        std::cerr << "scenario reset error=" << error << '\n';
        return false;
    }

    SimulationContext::itsStartTime = "2026-10-22T10:00:00";

    vanetza_integration::vam::VamGenerationConfig generation{};
    generation.tCheckVamGenMs = 100;
    generation.tGenVamMinMs = 100;
    generation.tGenVamMaxMs = 5000;
    generation.tGenVamLfMinMs = 2000;

    vanetza_integration::vam::VamGenerationEvaluationAdapterConfig evaluation{};
    evaluation.snapshotBuilder.vehicleThresholds = {2.0, 2.0, 1.0};
    evaluation.snapshotBuilder.vruThresholds = {2.0, 2.0, 1.0};
    evaluation.proximity.maximumSampleAgeUs = 250000;

    if (!Check(
            SimulationController::ConfigureVamGeneration(
                true, generation, evaluation, error),
            "VAM_SCHEDULER_CONFIGURED"))
    {
        std::cerr << "scheduler config error=" << error << '\n';
        return false;
    }

    VamVruScenarioSubjectConfig subject{};
    subject.personId = kPersonId;
    subject.stationId = kStationId;
    subject.activateRole = false;
    subject.vbsState = VamVruScenarioSubjectConfig::VbsState::Idle;

    if (!Check(
            SimulationController::ConfigureVamVruScenarioSubjects({subject}, error),
            "VRU_SCENARIO_IDENTITY_RESERVED"))
    {
        std::cerr << "scenario subject config error=" << error << '\n';
        return false;
    }

    const ItsStationIdentityOwner owner{
        ItsStationIdentityOwnerKind::ItsStation,
        std::string("vam-vru:") + kPersonId
    };
    if (!Check(
            authority.IsReservedBy(kStationId, owner),
            "VRU_IDENTITY_PRE_RESERVED"))
    {
        return false;
    }

    // Configure the same production Wi-Fi channel and advancing IPv4 allocator
    // used by vehicle, CPM and SPATEM endpoints.
    YansWifiChannelHelper channelHelper;
    channelHelper.SetPropagationDelay(
        "ns3::ConstantSpeedPropagationDelayModel");
    channelHelper.AddPropagationLoss(
        "ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(2.7),
        "ReferenceDistance", DoubleValue(1.0));
    Ptr<YansWifiChannel> channel = channelHelper.Create();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.91.0.0", "255.255.255.0");
    Callback<void, Ptr<Socket>> noReceiveCallback;
    VehicleFactory::Initialize(channel, ipv4, noReceiveCallback);

    if (!RunConcurrentPresenceEndpointOwnership())
    {
        return false;
    }

    SimulationController::ConfigureSumoAdapterCallbacks();
    const int localPort = FindAvailableUdpPort();
    if (!Check(localPort > 0, "SUMO_UDP_TEST_PORT_ALLOCATED"))
    {
        return false;
    }

    if (!Check(
            SumoAdapter::Init(localPort, "127.0.0.1", 7000),
            "REAL_SUMO_UDP_ADAPTER_INITIALIZED"))
    {
        return false;
    }
    cleanup.adapterInitialized = true;

    cleanup.senderFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!Check(cleanup.senderFd >= 0, "SUMO_UDP_INJECTOR_CREATED"))
    {
        return false;
    }

    bool passed = true;
    EndpointSnapshot first{};

    passed = Check(
        SendDatagram(
            cleanup.senderFd,
            localPort,
            "SUMO_PERSON,UPSERT,runtime-vru-lifecycle,20374.270,9640.360,1,90,1000,pedestrian"),
        "FIRST_UPSERT_SENT") && passed;

    const bool firstFound = FindVruEndpoint(kPersonId, first);
    passed = Check(
        firstFound &&
            first.stationId == kStationId &&
            first.kind == StationEndpointKind::ItsStation &&
            first.stationType == vanetza::geonet::StationType::Pedestrian,
        "FIRST_UPSERT_CREATED_ACTIVE_PEDESTRIAN_ENDPOINT") && passed;

    passed = Check(
        firstFound &&
            CheckMobility(
                first,
                Vector(20374.270, 9640.360, 0.0),
                Vector(1.0, 0.0, 0.0)),
        "FIRST_UPSERT_APPLIED_SUMO_HEADING_90_EAST_VELOCITY") && passed;

    const Ipv4Address firstIpv4 = first.ipv4;
    const std::uint32_t firstNodeId = first.nodeId;

    passed = Check(
        SendDatagram(
            cleanup.senderFd,
            localPort,
            "SUMO_PERSON,UPSERT,runtime-vru-lifecycle,20375.270,9640.360,2,0,1500,pedestrian"),
        "SECOND_UPSERT_SENT") && passed;

    EndpointSnapshot updated{};
    const bool updatedFound = FindVruEndpoint(kPersonId, updated);
    passed = Check(
        updatedFound &&
            updated.nodeId == firstNodeId &&
            updated.ipv4 == firstIpv4,
        "REPEATED_UPSERT_REUSED_SAME_NETWORK_ENDPOINT") && passed;
    passed = Check(
        updatedFound &&
            CheckMobility(
                updated,
                Vector(20375.270, 9640.360, 0.0),
                Vector(0.0, 2.0, 0.0)),
        "REPEATED_UPSERT_UPDATED_POSITION_AND_HEADING_0_NORTH_VELOCITY") && passed;

    // Out-of-order UPSERT must be rejected by the authoritative person
    // registry and must not alter the already-active endpoint.
    passed = Check(
        SendDatagram(
            cleanup.senderFd,
            localPort,
            "SUMO_PERSON,UPSERT,runtime-vru-lifecycle,99999.0,99999.0,9,180,1400,pedestrian"),
        "STALE_UPSERT_SENT") && passed;

    EndpointSnapshot afterStaleUpsert{};
    const auto personAfterStaleUpsert =
        SimulationController::GetSumoPersonRegistry().Find(kPersonId);
    const bool staleUpsertPreserved =
        FindVruEndpoint(kPersonId, afterStaleUpsert) &&
        afterStaleUpsert.nodeId == firstNodeId &&
        personAfterStaleUpsert.has_value() &&
        personAfterStaleUpsert->simulationTimeUs == 1500 &&
        CheckMobility(
            afterStaleUpsert,
            Vector(20375.270, 9640.360, 0.0),
            Vector(0.0, 2.0, 0.0));
    passed = Check(
        staleUpsertPreserved,
        "REJECTED_STALE_UPSERT_LEFT_ENDPOINT_AND_MOBILITY_UNCHANGED") && passed;

    // A REMOVE at the active sample timestamp is stale and must not destroy
    // the current endpoint.
    passed = Check(
        SendDatagram(
            cleanup.senderFd,
            localPort,
            "SUMO_PERSON,REMOVE,runtime-vru-lifecycle,1500"),
        "STALE_REMOVE_SENT") && passed;

    EndpointSnapshot afterStaleRemove{};
    passed = Check(
        FindVruEndpoint(kPersonId, afterStaleRemove) &&
            afterStaleRemove.nodeId == firstNodeId &&
            SimulationController::GetSumoPersonRegistry().Find(kPersonId).has_value(),
        "REJECTED_STALE_REMOVE_PRESERVED_ACTIVE_ENDPOINT") && passed;

    passed = Check(
        SendDatagram(
            cleanup.senderFd,
            localPort,
            "SUMO_PERSON,REMOVE,runtime-vru-lifecycle,2000"),
        "VALID_REMOVE_SENT") && passed;

    const StationEndpointIdentity* oldRegistryEntry =
        StationEndpointRegistry::Instance().GetByIpv4(firstIpv4);
    const auto identityAfterRemove = authority.FindStationId(owner);
    passed = Check(
        oldRegistryEntry == nullptr &&
            !SimulationController::GetSumoPersonRegistry().Find(kPersonId).has_value() &&
            !SimulationController::GetSumoVruSubjectRegistry().Find(kPersonId).has_value(),
        "VALID_REMOVE_DESTROYED_NETWORK_ENDPOINT_AND_ACTIVE_SUMO_SUBJECT") && passed;
    passed = Check(
        identityAfterRemove.has_value() && *identityAfterRemove == kStationId &&
            authority.IsReservedBy(kStationId, owner),
        "VALID_REMOVE_PRESERVED_GLOBAL_STATION_IDENTITY") && passed;

    passed = Check(
        SendDatagram(
            cleanup.senderFd,
            localPort,
            "SUMO_PERSON,UPSERT,runtime-vru-lifecycle,20376.270,9640.360,3,90,3000,pedestrian"),
        "REENTRY_UPSERT_SENT") && passed;

    EndpointSnapshot reentered{};
    const bool reentryFound = FindVruEndpoint(kPersonId, reentered);
    passed = Check(
        reentryFound &&
            reentered.stationId == kStationId &&
            reentered.kind == StationEndpointKind::ItsStation &&
            reentered.stationType == vanetza::geonet::StationType::Pedestrian &&
            reentered.nodeId != firstNodeId &&
            reentered.ipv4 != firstIpv4,
        "REENTRY_CREATED_NEW_ENDPOINT_WITH_PERSISTED_IDENTITY") && passed;
    passed = Check(
        reentryFound &&
            CheckMobility(
                reentered,
                Vector(20376.270, 9640.360, 0.0),
                Vector(3.0, 0.0, 0.0)),
        "REENTRY_APPLIED_FRESH_SUMO_MOBILITY_SAMPLE") && passed;

    passed = Check(
        SimulationController::GetSumoPersonRegistry().Find(kPersonId).has_value() &&
            authority.IsReservedBy(kStationId, owner),
        "REENTRY_RESTORED_ACTIVE_PERSON_WITH_SAME_STATION_ID") && passed;

    passed = cleanup.Cleanup() && passed;
    if (passed)
    {
        std::cout << "VAM_VRU_ENDPOINT_LIFECYCLE_RUNTIME=PASS\n";
    }
    else
    {
        std::cout << "VAM_VRU_ENDPOINT_LIFECYCLE_FAILURES=DETECTED\n";
    }
    return passed;
}

} // namespace

int main()
{
    const bool ownershipStoreOk = RunVamVruEndpointOwnershipStoreLifecycle();
    const bool lifecycleOk = RunLifecycleIntegration();
    return ownershipStoreOk && lifecycleOk ? 0 : 1;
}
