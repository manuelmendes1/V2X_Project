#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include "ns3/callback.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/vector.h"
#include "ns3/wifi-module.h"

#include <vanetza/geonet/station_type.hpp>

#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/vam/emitter/vam_vru_emitter_endpoint.h"

namespace
{

constexpr std::uint32_t kStationId = 910001;
constexpr std::uint32_t kConflictingStationId = 910002;
constexpr double kTolerance = 1e-9;

int g_failures = 0;

bool
Check(bool condition, const char* marker)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << marker << '\n';
        ++g_failures;
        return false;
    }

    std::cout << "[PASS] " << marker << '\n';
    return true;
}

bool
Near(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= kTolerance;
}

bool
SameVector(const ns3::Vector& lhs, const ns3::Vector& rhs)
{
    return Near(lhs.x, rhs.x) &&
           Near(lhs.y, rhs.y) &&
           Near(lhs.z, rhs.z);
}

ns3::VamVruEmitterConfig
ValidConfig()
{
    ns3::VamVruEmitterConfig config{};
    config.personId = "pedestrian-910001";
    config.stationId = kStationId;
    config.initialPosition = ns3::Vector(10.0, 20.0, 0.0);
    config.initialVelocity = ns3::Vector(1.5, -2.0, 0.0);
    return config;
}

ns3::ItsStationIdentityOwner
OwnerFor(const std::string& personId)
{
    return {
        ns3::ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:" + personId};
}

} // namespace

int
main()
{
    using namespace ns3;

    auto& authority = ItsStationIdentityAuthority::Instance();
    auto& endpointRegistry = StationEndpointRegistry::Instance();
    auto& vehicleRegistry = VehicleRegistry::Instance();

    endpointRegistry.Clear();
    vehicleRegistry.Clear();
    authority.Clear();

    const auto owner = OwnerFor("pedestrian-910001");

    Check(
        authority.Reserve(owner, kStationId),
        "SCENARIO_IDENTITY_PRE_RESERVED_EXPLICITLY");

    auto emptyPerson = ValidConfig();
    emptyPerson.personId.clear();
    Check(
        VamVruEmitterEndpoint::Create(emptyPerson) == nullptr,
        "EMPTY_PERSON_ID_REJECTED");

    auto zeroStation = ValidConfig();
    zeroStation.stationId = 0;
    Check(
        VamVruEmitterEndpoint::Create(zeroStation) == nullptr,
        "ZERO_STATION_ID_REJECTED");

    auto nonFinitePosition = ValidConfig();
    nonFinitePosition.initialPosition.x =
        std::numeric_limits<double>::quiet_NaN();
    Check(
        VamVruEmitterEndpoint::Create(nonFinitePosition) == nullptr,
        "NON_FINITE_INITIAL_POSITION_REJECTED");

    auto nonFiniteVelocity = ValidConfig();
    nonFiniteVelocity.initialVelocity.y =
        std::numeric_limits<double>::infinity();
    Check(
        VamVruEmitterEndpoint::Create(nonFiniteVelocity) == nullptr,
        "NON_FINITE_INITIAL_VELOCITY_REJECTED");

    const ItsStationIdentityOwner conflictingOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "vam-vru:preexisting-person"};
    Check(
        authority.Reserve(conflictingOwner, kConflictingStationId),
        "CONFLICT_FIXTURE_IDENTITY_RESERVED");

    auto conflictingConfig = ValidConfig();
    conflictingConfig.personId = "another-person";
    conflictingConfig.stationId = kConflictingStationId;
    Check(
        VamVruEmitterEndpoint::Create(conflictingConfig) == nullptr,
        "STATION_ID_OWNED_BY_ANOTHER_PERSON_REJECTED");

    auto changedIdentityConfig = ValidConfig();
    changedIdentityConfig.stationId = kConflictingStationId;
    Check(
        VamVruEmitterEndpoint::Create(changedIdentityConfig) == nullptr,
        "EXISTING_PERSON_CANNOT_CHANGE_STATION_ID");

    auto config = ValidConfig();
    auto endpoint = VamVruEmitterEndpoint::Create(config);
    Check(endpoint != nullptr, "PRE_RESERVED_VRU_ENDPOINT_CREATED");
    if (!endpoint)
    {
        Simulator::Destroy();
        endpointRegistry.Clear();
        vehicleRegistry.Clear();
        authority.Clear();
        return 1;
    }

    Check(
        endpoint->LogicalId() == "vam-vru:pedestrian-910001",
        "LOGICAL_ID_USES_CANONICAL_VAM_VRU_OWNER");
    Check(
        endpoint->StationId() == kStationId,
        "EXPLICIT_STATION_ID_PRESERVED");
    Check(
        endpoint->StationType() == vanetza::geonet::StationType::Pedestrian,
        "GEO_NETWORK_STATION_TYPE_IS_PEDESTRIAN");
    Check(
        endpoint->StationState().identity.logicalId == endpoint->LogicalId() &&
            endpoint->StationState().identity.stationId == kStationId &&
            endpoint->StationState().identity.stationType ==
                vanetza::geonet::StationType::Pedestrian,
        "ITS_STATION_STATE_IDENTITY_IS_CONSISTENT");
    Check(
        authority.IsReservedBy(kStationId, owner),
        "ENDPOINT_USES_SCENARIO_IDENTITY_RESERVATION");

    YansWifiChannelHelper channelHelper;
    channelHelper.SetPropagationDelay(
        "ns3::ConstantSpeedPropagationDelayModel");
    channelHelper.AddPropagationLoss("ns3::FriisPropagationLossModel");
    Ptr<YansWifiChannel> channel = channelHelper.Create();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.77.0.0", "255.255.255.0");
    Callback<void, Ptr<Socket>> rxCallback;

    Check(
        endpoint->InitializeNetwork(channel, ipv4, rxCallback),
        "VRU_NETWORK_INITIALIZED");

    const Ptr<Node> firstNode = endpoint->StationState().node;
    const Ipv4Address firstIpv4 = endpoint->StationState().ipv4Address;
    const Mac48Address firstMac = endpoint->StationState().linkMac;

    Check(
        endpoint->StationState().networkInitialized &&
            firstNode != nullptr &&
            endpoint->StationState().socket != nullptr &&
            firstIpv4 != Ipv4Address::GetAny() &&
            firstMac != Mac48Address(),
        "VRU_NODE_SOCKET_IPV4_AND_LINK_MAC_CREATED");

    const StationEndpointIdentity* registered =
        endpointRegistry.GetByIpv4(firstIpv4);
    Check(
        registered != nullptr &&
            registered->kind == StationEndpointKind::ItsStation &&
            registered->logicalId == endpoint->LogicalId() &&
            registered->vehicleId.empty() &&
            registered->stationId == kStationId &&
            registered->stationType ==
                vanetza::geonet::StationType::Pedestrian &&
            registered->linkMacInitialized,
        "IPV4_REGISTRY_CONTAINS_NON_VEHICLE_PEDESTRIAN_ENDPOINT");

    Check(
        vehicleRegistry.GetByStationId(kStationId) == nullptr,
        "VRU_ENDPOINT_DOES_NOT_CREATE_VEHICLE_REGISTRY_ENTRY");

    Ptr<ConstantVelocityMobilityModel> mobility =
        firstNode->GetObject<ConstantVelocityMobilityModel>();
    Check(
        mobility != nullptr,
        "VRU_ENDPOINT_INSTALLS_CONSTANT_VELOCITY_MOBILITY");

    if (mobility != nullptr)
    {
        Check(
            SameVector(mobility->GetPosition(), config.initialPosition),
            "INITIAL_SUMO_NS3_POSITION_APPLIED");
        Check(
            SameVector(mobility->GetVelocity(), config.initialVelocity),
            "INITIAL_VELOCITY_SURVIVES_NETWORK_POSITION_SETUP");

        const Vector updatedPosition(31.0, 42.0, 0.0);
        const Vector updatedVelocity(-0.5, 3.25, 0.0);
        Check(
            endpoint->UpdateMobility(updatedPosition, updatedVelocity),
            "VALID_MOBILITY_UPDATE_ACCEPTED");
        Check(
            SameVector(mobility->GetPosition(), updatedPosition),
            "MOBILITY_UPDATE_APPLIES_POSITION");
        Check(
            SameVector(mobility->GetVelocity(), updatedVelocity),
            "MOBILITY_UPDATE_APPLIES_VELOCITY_AFTER_POSITION");

        const Vector beforeInvalidPosition = mobility->GetPosition();
        const Vector beforeInvalidVelocity = mobility->GetVelocity();
        const Vector invalidPosition(
            std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0);
        Check(
            !endpoint->UpdateMobility(invalidPosition, Vector(0.0, 0.0, 0.0)),
            "NON_FINITE_MOBILITY_UPDATE_REJECTED");
        Check(
            SameVector(mobility->GetPosition(), beforeInvalidPosition) &&
                SameVector(mobility->GetVelocity(), beforeInvalidVelocity),
            "REJECTED_MOBILITY_UPDATE_PRESERVES_LAST_VALID_STATE");
    }

    Check(
        endpoint->InitializeNetwork(channel, ipv4, rxCallback),
        "NETWORK_INITIALIZATION_IS_IDEMPOTENT");
    Check(
        endpoint->StationState().node == firstNode &&
            endpoint->StationState().ipv4Address == firstIpv4,
        "REPEATED_INITIALIZATION_DOES_NOT_CREATE_SECOND_NODE");

    Check(
        vehicleRegistry.GetByStationId(kStationId) == nullptr,
        "MOBILITY_UPDATES_KEEP_VEHICLE_REGISTRY_UNTOUCHED");

    endpoint.reset();

    Check(
        endpointRegistry.GetByIpv4(firstIpv4) == nullptr,
        "ENDPOINT_DESTRUCTION_UNREGISTERS_IPV4_ENDPOINT");
    Check(
        authority.IsReservedBy(kStationId, owner),
        "ENDPOINT_DESTRUCTION_PRESERVES_GLOBAL_IDENTITY_RESERVATION");
    Check(
        vehicleRegistry.GetByStationId(kStationId) == nullptr,
        "ENDPOINT_DESTRUCTION_DOES_NOT_CREATE_VEHICLE_STATE");

    auto reentered = VamVruEmitterEndpoint::Create(config);
    Check(reentered != nullptr, "SAME_LOGICAL_VRU_CAN_REENTER");
    if (reentered)
    {
        Check(
            reentered->StationId() == kStationId &&
                reentered->LogicalId() == "vam-vru:pedestrian-910001",
            "REENTRY_REUSES_SAME_LOGICAL_STATION_IDENTITY");
        Check(
            reentered->InitializeNetwork(channel, ipv4, rxCallback),
            "REENTRY_CREATES_NEW_NETWORK_ENDPOINT");
        Check(
            reentered->StationState().node != nullptr &&
                reentered->StationState().node != firstNode &&
                endpointRegistry.GetByIpv4(
                    reentered->StationState().ipv4Address) != nullptr,
            "REENTRY_REGISTERS_NEW_NODE_WITH_PERSISTED_IDENTITY");
        Check(
            vehicleRegistry.GetByStationId(kStationId) == nullptr,
            "REENTRY_REMAINS_OUTSIDE_VEHICLE_REGISTRY");
    }

    reentered.reset();
    Check(
        authority.IsReservedBy(kStationId, owner),
        "REENTRY_ENDPOINT_DESTRUCTION_STILL_PRESERVES_IDENTITY");

    std::cout << "VAM_VRU_ENDPOINT_FAILURES=" << g_failures << '\n';
    if (g_failures == 0)
    {
        std::cout << "VAM_VRU_ENDPOINT_RUNTIME=PASS\n";
    }

    endpointRegistry.Clear();
    vehicleRegistry.Clear();
    authority.Clear();
    Simulator::Destroy();

    return g_failures == 0 ? 0 : 1;
}
