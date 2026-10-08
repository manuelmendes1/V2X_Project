#include <iostream>

#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/yans-wifi-channel.h"

namespace
{

void
NullRxCallback(ns3::Ptr<ns3::Socket>)
{
}

bool
Check(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        std::cerr
            << "FAIL "
            << message
            << "\n";
        return false;
    }

    std::cout
        << "PASS "
        << message
        << "\n";

    return true;
}

} // namespace

int
main()
{
    std::cout
        << "============================================================\n"
        << "SPATEM-19 / 9.14 — UNKNOWN SENDER RUNTIME TEST\n"
        << "============================================================\n";

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();

    auto channel =
        ns3::YansWifiChannelHelper::Default().Create();

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase(
        "10.0.0.0",
        "255.255.255.0");

    ns3::Callback<void, ns3::Ptr<ns3::Socket>> rxCallback =
        ns3::MakeCallback(&NullRxCallback);

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        rxCallback);

    ns3::VehicleFactory::CreateVehicle("veh1");

    auto* receiver =
        registry.GetByVehId("veh1");

    bool ok = true;

    ok &= Check(
        receiver != nullptr,
        "receiver vehicle created");

    if (!ok)
    {
        registry.Clear();
        return 1;
    }

    ok &= Check(
        receiver->runtime != nullptr &&
        receiver->runtime->IsInitialized(),
        "receiver runtime initialized");

    ok &= Check(
        registry.GetByIpv4(
            ns3::Ipv4Address("10.99.99.99")) == nullptr,
        "unknown source IPv4 is absent from VehicleRegistry");

    if (!ok)
    {
        registry.Clear();
        return 1;
    }

    auto packet =
        ns3::Create<ns3::Packet>(
            24);

    const ns3::InetSocketAddress unknownSource(
        ns3::Ipv4Address("10.99.99.99"),
        5000);

    std::cout
        << "BEFORE ReceivePacket unknown_source="
        << unknownSource.GetIpv4()
        << "\n";

    receiver->runtime->ReceivePacket(
        packet,
        unknownSource);

    std::cout
        << "PASS ReceivePacket returned without process abort\n";

    registry.Clear();

    std::cout
        << "SPATEM_UNKNOWN_SENDER_RUNTIME_EXIT=0\n";

    return 0;
}
