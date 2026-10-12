#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/rx/rx_callback/rx_callback.h"
#include "vanetza_integration/spatem/transport/spatem_transport_gateway.h"

#include "ns3/inet-socket-address.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/yans-wifi-channel.h"

namespace
{

uint32_t g_socketRxPackets = 0;
uint32_t g_targetReceiverNodeId = UINT32_MAX;

void
RealRxCallback(ns3::Ptr<ns3::Socket> socket)
{
    if (socket == nullptr)
    {
        return;
    }

    ns3::Ptr<ns3::Packet> packet;
    ns3::Address from;

    while ((packet = socket->RecvFrom(from)))
    {
        ++g_socketRxPackets;

        std::cout
            << "[SPATEM_RX_TEST] socket_rx"
            << " node="
            << socket->GetNode()->GetId()
            << " size="
            << packet->GetSize()
            << " from="
            << from
            << "\n";

        if (socket->GetNode()->GetId() != g_targetReceiverNodeId)
        {
            continue;
        }

        auto* receiver =
            ns3::VehicleRegistry::Instance()
                .GetByNodeId(g_targetReceiverNodeId);

        if (receiver == nullptr)
        {
            std::cerr
                << "FAIL receiver not found in registry\n";
            continue;
        }

        if (receiver->runtime == nullptr ||
            !receiver->runtime->IsInitialized())
        {
            std::cerr
                << "FAIL receiver runtime not initialized\n";
            continue;
        }

        receiver->runtime->ReceivePacket(
            packet,
            from);
    }
}

bool
Check(bool condition, const std::string& message)
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
    bool ok = true;

    std::cout
        << "============================================================\n"
        << "ETAPA 9.6.6 — REAL SPATEM RX ACCEPTANCE TEST\n"
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
        ns3::MakeCallback(&RealRxCallback);

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        rxCallback);

    ns3::Ptr<ns3::Node> node0 =
        ns3::VehicleFactory::CreateVehicle("veh0");

    ns3::Ptr<ns3::Node> node1 =
        ns3::VehicleFactory::CreateVehicle("veh1");

    auto* veh0 =
        registry.GetByVehId("veh0");

    auto* veh1 =
        registry.GetByVehId("veh1");

    ok &= Check(
        node0 != nullptr &&
        node1 != nullptr,
        "two real VehicleFactory nodes created");

    ok &= Check(
        veh0 != nullptr &&
        veh1 != nullptr,
        "veh0 and veh1 registered");

    if (!ok)
    {
        registry.Clear();
        return 1;
    }

    ok &= Check(
        veh0->runtime != nullptr &&
        veh0->runtime->IsInitialized(),
        "veh0 VanetzaRuntime initialized");

    ok &= Check(
        veh1->runtime != nullptr &&
        veh1->runtime->IsInitialized(),
        "veh1 VanetzaRuntime initialized");

    ok &= Check(
        veh0->ipv4Initialized &&
        veh1->ipv4Initialized,
        "both vehicles have registered IPv4 addresses");

    ok &= Check(
        veh0->linkMacInitialized &&
        veh1->linkMacInitialized,
        "both vehicles have initialized Link MAC");

    if (!ok)
    {
        registry.Clear();
        return 1;
    }

    g_targetReceiverNodeId =
        veh1->identity.nodeId;

    auto mobility0 =
        node0->GetObject<
            ns3::ConstantVelocityMobilityModel>();

    auto mobility1 =
        node1->GetObject<
            ns3::ConstantVelocityMobilityModel>();

    ok &= Check(
        mobility0 != nullptr &&
        mobility1 != nullptr,
        "both vehicles expose mobility models");

    if (mobility0)
    {
        mobility0->SetPosition(
            ns3::Vector(0.0, 0.0, 0.0));

        mobility0->SetVelocity(
            ns3::Vector(0.0, 0.0, 0.0));
    }

    if (mobility1)
    {
        mobility1->SetPosition(
            ns3::Vector(1.0, 0.0, 0.0));

        mobility1->SetVelocity(
            ns3::Vector(0.0, 0.0, 0.0));
    }

    /*
     * Transport-only payload.
     *
     * 9.6.6 deliberately does not decode SPATEM. The objective is to prove
     * real BTP/GeoNetworking RX dispatch for port 2004 and preservation of
     * the actual wire payload. ASN.1 decode is deferred to later stages.
     */
    const std::vector<uint8_t> payload{
        0x10, 0x20, 0x30, 0x40
    };

    ns3::PacketContext txContext{};

    ns3::SpatemTransportGateway gateway(
        [veh0](
            ns3::PacketContext& context,
            const ns3::BtpTxRequest& request,
            const std::vector<uint8_t>& data)
        {
            return veh0->runtime->SendBtpB(
                context,
                request,
                data);
        });

    ns3::Simulator::Schedule(
        ns3::MilliSeconds(100),
        [&]()
        {
            const bool sent =
                gateway.Send(
                    txContext,
                    payload);

            std::cout
                << "[SPATEM_RX_TEST] tx_result="
                << (sent ? "PASS" : "FAIL")
                << "\n";

            std::cout
                << "[SPATEM_RX_TEST] tx_stage="
                << static_cast<int>(
                    txContext.pipelineStage)
                << "\n";
        });

    ns3::Simulator::Schedule(
        ns3::MilliSeconds(500),
        []()
        {
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    ok &= Check(
        txContext.pipelineStage >=
            ns3::PipelineStage::GEONET_TX,
        "real SPATEM TX reached GEONET_TX");

    ok &= Check(
        txContext.pipelineResult ==
            ns3::PipelineResult::OK,
        "real SPATEM TX transport accepted");

    ok &= Check(
        g_socketRxPackets > 0,
        "real ns-3 socket received at least one packet");

    std::cout
        << "SPATEM_SOCKET_RX_PACKETS="
        << g_socketRxPackets
        << "\n";

    /*
     * The acceptance shell validates the negative SPATEM path after the
     * real socket/RX path reaches the SPATEM runtime boundary.
     */
    std::cout
        << "SPATEM_EXPECTATION="
        << "invalid 4-byte SPATEM payload must be rejected at DECODE"
        << "\n";

    registry.Clear();

    /*
     * This executable proves the real socket/RX path. The enclosing
     * acceptance script validates BTP 2004 dispatch and the expected
     * rejection of the intentionally invalid SPATEM payload.
     */
    return ok ? 0 : 1;
}
