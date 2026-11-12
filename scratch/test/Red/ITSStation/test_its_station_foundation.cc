#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include <vanetza/common/clock.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/geonet/address.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/net/mac_address.hpp>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/wifi-module.h"

#include "vanetza_integration/context/its_station/state/its_station_state.h"
#include "vanetza_integration/context/its_station/address/its_station_address_builder.h"
#include "vanetza_integration/context/its_station/network/its_station_network_builder.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"

namespace
{
using namespace ns3;

void Require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void TestItsStationState()
{
    const ItsStationState state{};

    Require(state.identity.logicalId.empty(),
            "ITS station logical ID must default to empty");
    Require(state.identity.stationId == 0,
            "ITS station ID must default to zero");
    Require(state.identity.stationType == vanetza::geonet::StationType::Unknown,
            "ITS station type must default to Unknown");
    Require(state.node == nullptr, "ITS station node must default to null");
    Require(state.socket == nullptr, "ITS station socket must default to null");
    Require(state.ipv4Address == Ipv4Address::GetAny(),
            "ITS station IPv4 must default to ANY");
    Require(!state.networkInitialized,
            "ITS station network must default to uninitialized");

    std::cout << "ITS_STATION_STATE=PASS\n";
}

void TestItsStationAddressBuilder()
{
    ItsStationIdentity rsu{};
    rsu.logicalId = "address-test-rsu";
    rsu.stationId = 9001;
    rsu.stationType = vanetza::geonet::StationType::RSU;

    vanetza::geonet::Address rsuAddress;
    Require(ItsStationAddressBuilder::Build(rsu, rsuAddress),
            "RSU GN_ADDR construction failed");
    Require(rsuAddress.station_type() == vanetza::geonet::StationType::RSU,
            "RSU station type was not preserved");
    Require(rsuAddress.is_manually_configured(),
            "GN_ADDR must be marked manually configured");
    Require(rsuAddress.country_code().raw() == 0,
            "GN_ADDR country code must retain the configured zero value");
    Require(rsuAddress.mid().octets[0] == 0x02 &&
                rsuAddress.mid().octets[1] == 0x00,
            "GN_ADDR MID must retain the deterministic 02:00 prefix");
    Require(rsuAddress.mid().octets[2] == 0x23 &&
                rsuAddress.mid().octets[3] == 0x29,
            "GN_ADDR MID must encode stationId 9001 in the expected octets");

    ItsStationIdentity pedestrian{};
    pedestrian.logicalId = "address-test-pedestrian";
    pedestrian.stationId = 9010;
    pedestrian.stationType = vanetza::geonet::StationType::Pedestrian;

    vanetza::geonet::Address pedestrianAddress;
    Require(ItsStationAddressBuilder::Build(pedestrian, pedestrianAddress),
            "Pedestrian GN_ADDR construction failed");
    Require(pedestrianAddress.station_type() ==
                vanetza::geonet::StationType::Pedestrian,
            "Pedestrian station type was not preserved");

    vanetza::geonet::Address sentinel;
    sentinel.mid(vanetza::MacAddress{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff});
    sentinel.station_type(vanetza::geonet::StationType::Cyclist);
    sentinel.country_code(vanetza::BitNumber<unsigned, 10>(42));
    sentinel.is_manually_configured(false);
    const vanetza::geonet::Address unchanged = sentinel;

    ItsStationIdentity zeroId{};
    zeroId.logicalId = "invalid-zero-id";
    zeroId.stationType = vanetza::geonet::StationType::RSU;
    Require(!ItsStationAddressBuilder::Build(zeroId, sentinel),
            "Address builder must reject stationId zero");
    Require(sentinel == unchanged,
            "Failed address construction must leave output unchanged");

    ItsStationIdentity unknownType{};
    unknownType.logicalId = "invalid-unknown-type";
    unknownType.stationId = 9011;
    unknownType.stationType = vanetza::geonet::StationType::Unknown;
    Require(!ItsStationAddressBuilder::Build(unknownType, sentinel),
            "Address builder must reject StationType::Unknown");
    Require(sentinel == unchanged,
            "Unknown station type failure must leave output unchanged");

    std::cout << "ITS_STATION_ADDRESS=PASS\n";
}

void TestStationEndpointRegistry()
{
    auto& registry = StationEndpointRegistry::Instance();
    auto& identityAuthority = ItsStationIdentityAuthority::Instance();
    registry.Clear();
    identityAuthority.Clear();

    const Ipv4Address stationIp("10.42.0.1");
    const Ipv4Address vehicleIp("10.42.0.2");
    const Mac48Address stationMac("02:00:00:00:00:01");
    const Mac48Address vehicleMac("02:00:00:00:00:02");

    const ItsStationIdentityOwner pedestrianOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "registry-pedestrian"
    };
    const ItsStationIdentityOwner ipv4CollisionOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "registry-collision"
    };

    Require(identityAuthority.Reserve(pedestrianOwner, 701),
            "Pedestrian station identity reservation failed");
    Require(identityAuthority.Reserve(ipv4CollisionOwner, 702),
            "IPv4 collision endpoint identity reservation failed");

    Require(!registry.RegisterItsStation(
                "unreserved-station", 16, 700,
                vanetza::geonet::StationType::Pedestrian,
                Ipv4Address("10.42.0.9"), stationMac),
            "ITS station registration must reject an unreserved stationId");

    Require(registry.RegisterItsStation(
                "registry-pedestrian", 17, 701,
                vanetza::geonet::StationType::Pedestrian,
                stationIp, stationMac),
            "Valid ITS station registration was rejected");

    const StationEndpointIdentity* station = registry.GetByIpv4(stationIp);
    Require(station != nullptr, "Registered ITS station was not resolved by IPv4");
    Require(station->kind == StationEndpointKind::ItsStation,
            "ITS station endpoint kind mismatch");
    Require(station->logicalId == "registry-pedestrian" &&
                station->stationId == 701 && station->nodeId == 17,
            "ITS station endpoint identity mismatch");
    Require(station->stationType == vanetza::geonet::StationType::Pedestrian,
            "ITS station endpoint type mismatch");
    Require(station->linkMacInitialized && station->linkMac == stationMac,
            "ITS station link MAC was not preserved");

    Require(!registry.RegisterItsStation(
                "registry-collision", 18, 702,
                vanetza::geonet::StationType::Cyclist,
                stationIp, Mac48Address("02:00:00:00:00:03")),
            "A different endpoint must not take an occupied IPv4 address");
    station = registry.GetByIpv4(stationIp);
    Require(station != nullptr && station->logicalId == "registry-pedestrian",
            "IPv4 collision must preserve the original endpoint");

    registry.UnregisterItsStation(stationIp, "wrong-logical-id", 701);
    Require(registry.GetByIpv4(stationIp) != nullptr,
            "Unregister with the wrong logical ID removed the endpoint");
    registry.UnregisterItsStation(stationIp, "registry-pedestrian", 702);
    Require(registry.GetByIpv4(stationIp) != nullptr,
            "Unregister with the wrong station ID removed the endpoint");
    registry.UnregisterItsStation(stationIp, "registry-pedestrian", 701);
    Require(registry.GetByIpv4(stationIp) == nullptr,
            "Matching ITS station unregister did not remove the endpoint");
    Require(identityAuthority.IsReservedBy(701, pedestrianOwner),
            "IPv4 unregister must not release the logical ITS identity");

    const ItsStationIdentityOwner vehicleOwner{
        ItsStationIdentityOwnerKind::Vehicle,
        "registry-vehicle"
    };
    const ItsStationIdentityOwner invalidMacVehicleOwner{
        ItsStationIdentityOwnerKind::Vehicle,
        "invalid-vehicle"
    };
    const ItsStationIdentityOwner otherItsStationOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "other-station-owner"
    };
    const ItsStationIdentityOwner invalidAnyIpOwner{
        ItsStationIdentityOwnerKind::ItsStation,
        "invalid-any-ip"
    };

    Require(identityAuthority.Reserve(vehicleOwner, 703),
            "Vehicle identity reservation failed");
    Require(identityAuthority.Reserve(invalidMacVehicleOwner, 704),
            "Invalid-MAC vehicle identity reservation failed");
    Require(identityAuthority.Reserve(otherItsStationOwner, 708),
            "Cross-owner station identity reservation failed");
    Require(identityAuthority.Reserve(invalidAnyIpOwner, 706),
            "Invalid-ANY-IP station identity reservation failed");

    Require(!registry.RegisterItsStation(
                "vehicle-id-owner-mismatch", 24, 703,
                vanetza::geonet::StationType::Cyclist,
                Ipv4Address("10.42.0.6"), stationMac),
            "ITS station must not claim a stationId reserved by a vehicle");

    Require(registry.RegisterVehicle(
                "registry-vehicle", 19, 703,
                vanetza::geonet::StationType::Passenger_Car,
                vehicleIp, vehicleMac, true),
            "Valid vehicle registration was rejected");
    const StationEndpointIdentity* vehicle = registry.GetByIpv4(vehicleIp);
    Require(vehicle != nullptr && vehicle->kind == StationEndpointKind::Vehicle,
            "Vehicle endpoint was not distinguishable from an ITS station");
    Require(vehicle->vehicleId == "registry-vehicle" &&
                vehicle->stationId == 703 && vehicle->linkMac == vehicleMac,
            "Vehicle endpoint identity mismatch");

    Require(!registry.RegisterVehicle(
                "unreserved-vehicle", 24, 707,
                vanetza::geonet::StationType::Passenger_Car,
                Ipv4Address("10.42.0.7"), vehicleMac, true),
            "Vehicle registration must reject an unreserved stationId");

    Require(!registry.RegisterVehicle(
                "its-station-id-owner-mismatch", 25, 708,
                vanetza::geonet::StationType::Passenger_Car,
                Ipv4Address("10.42.0.8"), vehicleMac, true),
            "Vehicle must not claim a stationId reserved by an ITS station");

    Require(!registry.RegisterVehicle(
                "invalid-vehicle", 20, 704,
                vanetza::geonet::StationType::Passenger_Car,
                Ipv4Address("10.42.0.3"), vehicleMac, false),
            "Vehicle registration must reject an uninitialized link MAC");
    Require(!registry.RegisterItsStation(
                "", 21, 705, vanetza::geonet::StationType::Pedestrian,
                Ipv4Address("10.42.0.4"), stationMac),
            "ITS station registration must reject an empty logical ID");
    Require(!registry.RegisterItsStation(
                "invalid-zero-station", 22, 0,
                vanetza::geonet::StationType::Pedestrian,
                Ipv4Address("10.42.0.5"), stationMac),
            "ITS station registration must reject stationId zero");
    Require(!registry.RegisterItsStation(
                "invalid-any-ip", 23, 706,
                vanetza::geonet::StationType::Pedestrian,
                Ipv4Address::GetAny(), stationMac),
            "ITS station registration must reject IPv4 ANY");

    registry.Clear();
    Require(registry.GetByIpv4(vehicleIp) == nullptr,
            "Registry Clear did not remove all endpoint entries");
    Require(identityAuthority.IsReservedBy(703, vehicleOwner),
            "Registry Clear must not release logical station identities");

    identityAuthority.Clear();
    Require(!identityAuthority.FindStationId(vehicleOwner).has_value(),
            "Identity authority Clear did not remove vehicle reservation");

    std::cout << "STATION_ENDPOINT_REGISTRY=PASS\n";
}

void TestNetworkBuilderAndAccessAdapter()
{
    YansWifiChannelHelper channelHelper;
    channelHelper.SetPropagationDelay(
        "ns3::ConstantSpeedPropagationDelayModel");
    channelHelper.AddPropagationLoss(
        "ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(2.7),
        "ReferenceDistance", DoubleValue(1.0));
    Ptr<YansWifiChannel> channel = channelHelper.Create();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.43.0.0", "255.255.255.0");
    Callback<void, Ptr<Socket>> rxCallback;

    ItsStationNetworkConfig fixedConfig{};
    fixedConfig.initialPosition = Vector(10.0, 20.0, 0.0);
    fixedConfig.phyAuditRole = PhyAuditRole::Rsu;
    fixedConfig.mobilityInstaller = [](Ptr<Node> node)
    {
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(node);
    };

    const ItsStationNetworkResources fixed =
        ItsStationNetworkBuilder::Build(channel, ipv4, rxCallback, fixedConfig);

    Require(fixed.node != nullptr && fixed.socket != nullptr,
            "Fixed ITS station network resources are incomplete");
    Require(fixed.ipv4Address != Ipv4Address::GetAny(),
            "Fixed ITS station IPv4 address is ANY");
    Require(fixed.linkMac != Mac48Address::GetBroadcast(),
            "Fixed ITS station link MAC is broadcast");

    Ptr<ConstantPositionMobilityModel> fixedMobility =
        fixed.node->GetObject<ConstantPositionMobilityModel>();
    Require(fixedMobility != nullptr,
            "Fixed mobility installer was not preserved by the network builder");
    Require(std::abs(fixedMobility->GetPosition().x - 10.0) < 1e-9 &&
                std::abs(fixedMobility->GetPosition().y - 20.0) < 1e-9,
            "Fixed mobility initial position mismatch");

    ItsStationNetworkConfig movingConfig{};
    movingConfig.initialPosition = Vector(20.0, 30.0, 0.0);
    movingConfig.phyAuditRole = PhyAuditRole::Vru;
    movingConfig.mobilityInstaller = [](Ptr<Node> node)
    {
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(node);
        Ptr<ConstantVelocityMobilityModel> model =
            node->GetObject<ConstantVelocityMobilityModel>();
        if (model == nullptr)
        {
            throw std::runtime_error(
                "ConstantVelocityMobilityModel was not installed");
        }
        model->SetVelocity(Vector(2.0, 0.0, 0.0));
    };

    const ItsStationNetworkResources moving =
        ItsStationNetworkBuilder::Build(channel, ipv4, rxCallback, movingConfig);

    Require(moving.node != nullptr && moving.socket != nullptr,
            "Moving ITS station network resources are incomplete");
    Require(moving.ipv4Address != Ipv4Address::GetAny(),
            "Moving ITS station IPv4 address is ANY");
    Require(moving.ipv4Address != fixed.ipv4Address,
            "ITS stations received the same IPv4 address");

    Ptr<ConstantVelocityMobilityModel> movingMobility =
        moving.node->GetObject<ConstantVelocityMobilityModel>();
    Require(movingMobility != nullptr,
            "Dynamic mobility installer was not preserved by the network builder");
    Require(std::abs(movingMobility->GetPosition().x - 20.0) < 1e-9 &&
                std::abs(movingMobility->GetPosition().y - 30.0) < 1e-9,
            "Dynamic mobility initial position mismatch");
    Require(std::abs(movingMobility->GetVelocity().x - 2.0) < 1e-9,
            "Dynamic mobility velocity was not preserved");

    bool moved = false;
    Simulator::Schedule(Seconds(1.0), [&]()
    {
        const double x = movingMobility->GetPosition().x;
        moved = std::abs(x - 22.0) < 0.05;
    });
    Simulator::Stop(Seconds(1.01));
    Simulator::Run();

    Require(moved, "Dynamic ITS station did not move during simulation time");
    Require(std::abs(fixedMobility->GetPosition().x - 10.0) < 1e-9,
            "Fixed RSU mobility changed during simulation time");

    {
        vanetza::ManualRuntime runtime(vanetza::Clock::time_point{});
        vanetza::geonet::MIB mib;
        vanetza::geonet::Router router(runtime, mib);

        Ns3AccessAdapter genericAdapter(
            fixed.node, fixed.socket, mib, router);
        Require(genericAdapter.GetWifiPhy() != nullptr,
                "Generic Node/Socket Access Adapter did not resolve Wi-Fi PHY");

        ItsStationState compatibilityState{};
        compatibilityState.node = fixed.node;
        compatibilityState.socket = fixed.socket;
        Ns3AccessAdapter compatibilityAdapter(
            compatibilityState, mib, router);
        Require(compatibilityAdapter.GetWifiPhy() != nullptr,
                "Legacy ITS Station Access Adapter constructor regressed");
    }

    Simulator::Destroy();
    std::cout << "ITS_STATION_NETWORK=PASS\n";
    std::cout << "NS3_ACCESS_ADAPTER_GENERIC=PASS\n";
}

} // namespace

int main()
{
    try
    {
        TestItsStationState();
        TestItsStationAddressBuilder();
        TestStationEndpointRegistry();
        TestNetworkBuilderAndAccessAdapter();
        std::cout << "ITS_STATION_FOUNDATION=PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ITS_STATION_FOUNDATION_FAIL=" << error.what() << '\n';
        Simulator::Destroy();
        return 1;
    }
}
