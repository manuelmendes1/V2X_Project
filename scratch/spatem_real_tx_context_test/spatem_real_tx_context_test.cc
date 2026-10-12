
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/spatem/service/spatem_service.h"
#include "vanetza_integration/spatem/transport/spatem_transport_gateway.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/yans-wifi-channel.h"

namespace
{

uint32_t g_socketRxPackets = 0;
uint32_t g_targetReceiverNodeId = UINT32_MAX;

bool
Check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << "\n";
        return false;
    }

    std::cout << "PASS " << message << "\n";
    return true;
}

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
            << "[SPATEM_REAL_TX_CONTEXT_TEST] socket_rx"
            << " node=" << socket->GetNode()->GetId()
            << " size=" << packet->GetSize()
            << "\n";

        if (socket->GetNode()->GetId() !=
            g_targetReceiverNodeId)
        {
            continue;
        }

        auto* receiver =
            ns3::VehicleRegistry::Instance()
                .GetByNodeId(g_targetReceiverNodeId);

        if (receiver == nullptr ||
            receiver->runtime == nullptr ||
            !receiver->runtime->IsInitialized())
        {
            std::cerr
                << "FAIL receiver/runtime unavailable\n";
            continue;
        }

        receiver->runtime->ReceivePacket(
            packet,
            from);
    }
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
    intersection.minuteOfYear = 1000;
    intersection.dSecond = 1000;

    SpatemMovement movement{};
    movement.signalGroup = 3;

    SpatemMovementEvent event{};
    event.phaseState =
        SpatemMovementPhaseState::
            PROTECTED_MOVEMENT_ALLOWED;

    SpatemTiming timing{};
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
        << "ETAPA 9.10 — REAL SPATEM PACKETCONTEXT TX TEST\n"
        << "============================================================\n";

    SimulationContext::itsStartTime =
        "2026-10-07T11:30:00";

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

    ns3::VehicleFactory::CreateVehicle("veh0");
    ns3::VehicleFactory::CreateVehicle("veh1");

    auto* veh0 =
        registry.GetByVehId("veh0");

    auto* veh1 =
        registry.GetByVehId("veh1");

    ok &= Check(
        veh0 != nullptr &&
        veh1 != nullptr,
        "veh0 and veh1 created");

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

    g_targetReceiverNodeId =
        veh1->identity.nodeId;

    const SpatemInput input =
        MakeDeterministicSpatem();

    auto context =
        std::make_shared<ns3::PacketContext>();

    context->senderVehId = "veh0";
    context->senderNodeId = veh0->identity.nodeId;
    context->senderStationId = veh0->identity.stationId;
    context->packetId = 9106;

    ns3::SpatemTransportGateway gateway(
        [veh0](
            ns3::PacketContext& ctx,
            const ns3::BtpTxRequest& request,
            const std::vector<uint8_t>& payload)
        {
            return veh0->runtime->SendBtpB(
                ctx,
                request,
                payload);
        });

    bool serviceSent = false;

    ns3::Simulator::Schedule(
        ns3::MilliSeconds(100),
        [&]()
        {
            const bool prepared =
                veh0->runtime &&
                veh0->runtime->PrepareTx();

            if (!prepared)
            {
                std::cerr
                    << "FAIL runtime TX preparation failed\n";
                serviceSent = false;
                return;
            }

            serviceSent =
                ns3::SpatemService::Send(
                    context,
                    input,
                    gateway,
                    "deterministic_9_10");
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
        serviceSent,
        "SpatemService accepted real SPATEM TX");

    ok &= Check(
        context->payload.type ==
            ns3::PayloadType::SPATEM,
        "same PacketContext retains payload type SPATEM");

    ok &= Check(
        !context->payload.bytes.empty(),
        "same PacketContext retains encoded SPATEM payload");

    ok &= Check(
        context->simulationTime >= 0.099,
        "same PacketContext retains TX simulation time");

    ok &= Check(
        context->timestamp >= 99000,
        "same PacketContext retains TX timestamp");

    ok &= Check(
        context->pipelineStage ==
            ns3::PipelineStage::TX_COMPLETE,
        "same PacketContext reaches TX_COMPLETE");

    ok &= Check(
        context->pipelineResult ==
            ns3::PipelineResult::OK,
        "same PacketContext reports successful TX_COMPLETE");

    ok &= Check(
        g_socketRxPackets >= 1,
        "real ns-3 socket observed SPATEM transmission");

    std::cout
        << "SPATEM_REAL_TX_CONTEXT_SOCKET_PACKETS="
        << g_socketRxPackets
        << "\n";

    std::cout
        << "SPATEM_REAL_TX_CONTEXT_GREEN_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    registry.Clear();

    return ok ? 0 : 1;
}
