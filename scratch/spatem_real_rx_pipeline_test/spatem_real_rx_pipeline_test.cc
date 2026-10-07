#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/spatem/transport/spatem_transport_gateway.h"
#include "vanetza_integration/spatem/types/spatem_input.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

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
            << "[SPATEM_REAL_RX_TEST] socket_rx"
            << " node=" << socket->GetNode()->GetId()
            << " size=" << packet->GetSize()
            << " from=" << from
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
                << "FAIL receiver not found\n";
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

SpatemInput
MakeDeterministicSpatem()
{
    SpatemInput input{};
    input.stationId = 1001;

    SpatemIntersection intersection{};
    intersection.intersectionId = 42;
    intersection.revision = 7;
    intersection.statusBits = 0;

    SpatemMovement movement{};
    movement.signalGroup = 3;

    SpatemMovementEvent event{};
    event.phaseState =
        SpatemMovementPhaseState::
            PROTECTED_MOVEMENT_ALLOWED;

    SpatemTiming timing{};
    timing.startTime = 50;
    timing.minEndTime = 100;
    timing.maxEndTime = 200;

    event.timing = timing;
    movement.events.push_back(event);
    intersection.movements.push_back(movement);
    input.intersections.push_back(intersection);

    return input;
}

} // namespace

int
main()
{
    bool ok = true;

    std::cout
        << "============================================================\n"
        << "ETAPA 9.8 — REAL SPATEM RX PIPELINE TEST\n"
        << "============================================================\n";

    SimulationContext::itsStartTime =
        "2026-10-07T11:00:00";

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
        "veh0 runtime initialized");

    ok &= Check(
        veh1->runtime != nullptr &&
        veh1->runtime->IsInitialized(),
        "veh1 runtime initialized");

    ok &= Check(
        veh0->ipv4Initialized &&
        veh1->ipv4Initialized,
        "both vehicles have IPv4");

    ok &= Check(
        veh0->linkMacInitialized &&
        veh1->linkMacInitialized,
        "both vehicles have Link MAC");

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

    const auto input =
        MakeDeterministicSpatem();

    const auto encoded =
        VanetzaSpatemCodec::Encode(input);

    ok &= Check(
        encoded.success,
        "real SPATEM payload encoded");

    ok &= Check(
        !encoded.bytes.empty(),
        "real SPATEM wire payload is non-empty");

    if (!ok)
    {
        registry.Clear();
        return 1;
    }

    std::cout
        << "[SPATEM_REAL_RX_TEST] encoded_bytes="
        << encoded.bytes.size()
        << "\n";

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
                    encoded.bytes);

            std::cout
                << "[SPATEM_REAL_RX_TEST] tx_result="
                << (sent ? "PASS" : "FAIL")
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
        "real ns-3 socket received the SPATEM frame");

    std::cout
        << "SPATEM_REAL_RX_SOCKET_PACKETS="
        << g_socketRxPackets
        << "\n";

    std::cout
        << "SPATEM_REAL_RX_EXPECTED_SEMANTIC_PIPELINE="
        << "SPATEM_DECODE_OK,SPATEM_RECEPTION_OK,SPATEM_DELIVER_OK"
        << "\n";

    registry.Clear();

    return ok ? 0 : 1;
}
