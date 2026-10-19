#pragma once

#include "vanetza_integration/core/packet_id_tag/packet_id_tag.h"

#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/simulator.h"
#include "ns3/wifi-module.h"

#include <cstdlib>
#include <string>

namespace ns3
{

// Optional diagnostic hooks. Normal behavior is unchanged unless
// V2X_PHY_AUDIT=1 is present in the process environment.
class PhyAudit final
{
public:
    static void ConnectRsu(Ptr<Node> node, const NetDeviceContainer& devices)
    {
        Connect(node, devices, "RSU");
    }

    static void ConnectVehicle(Ptr<Node> node, const NetDeviceContainer& devices)
    {
        Connect(node, devices, "VEHICLE");
    }

private:
    static bool Enabled()
    {
        const char* value = std::getenv("V2X_PHY_AUDIT");
        return value != nullptr && std::string(value) != "" &&
               std::string(value) != "0";
    }

    static std::string PacketIdField(Ptr<const Packet> packet)
    {
        PacketIdTag tag;
        if (packet != nullptr && packet->PeekPacketTag(tag))
        {
            return " packet_id=" + std::to_string(tag.GetPacketId());
        }
        return " packet_id=missing";
    }

    static uint32_t PacketSize(Ptr<const Packet> packet)
    {
        return packet != nullptr ? packet->GetSize() : 0;
    }

    static void Connect(Ptr<Node> node,
                        const NetDeviceContainer& devices,
                        const std::string& role)
    {
        if (!Enabled() || node == nullptr)
        {
            return;
        }

        for (uint32_t i = 0; i < devices.GetN(); ++i)
        {
            Ptr<WifiNetDevice> wifi =
                DynamicCast<WifiNetDevice>(devices.Get(i));
            if (wifi == nullptr || wifi->GetPhy() == nullptr)
            {
                continue;
            }

            Ptr<WifiPhy> phy = wifi->GetPhy();
            const uint32_t nodeId = node->GetId();

            const bool txBegin = phy->TraceConnectWithoutContext(
                "PhyTxBegin",
                MakeBoundCallback(&PhyAudit::TxBegin, role, nodeId, i));
            const bool txEnd = phy->TraceConnectWithoutContext(
                "PhyTxEnd",
                MakeBoundCallback(&PhyAudit::TxEnd, role, nodeId, i));
            const bool rxBegin = phy->TraceConnectWithoutContext(
                "PhyRxBegin",
                MakeBoundCallback(&PhyAudit::RxBegin, role, nodeId, i));
            const bool rxEnd = phy->TraceConnectWithoutContext(
                "PhyRxEnd",
                MakeBoundCallback(&PhyAudit::RxEnd, role, nodeId, i));
            const bool rxDrop = phy->TraceConnectWithoutContext(
                "PhyRxDrop",
                MakeBoundCallback(&PhyAudit::RxDrop, role, nodeId, i));

            NS_LOG_UNCOND(
                "[PHY_AUDIT] event=TRACE_CONNECT"
                << " role=" << role
                << " node=" << nodeId
                << " device=" << i
                << " tx_begin=" << txBegin
                << " tx_end=" << txEnd
                << " rx_begin=" << rxBegin
                << " rx_end=" << rxEnd
                << " rx_drop=" << rxDrop);
        }
    }

    static void TxBegin(std::string role,
                        uint32_t nodeId,
                        uint32_t deviceIndex,
                        Ptr<const Packet> packet,
                        double txPowerW)
    {
        NS_LOG_UNCOND(
            "[PHY_AUDIT] event=PHY_TX_BEGIN"
            << " role=" << role
            << " node=" << nodeId
            << " device=" << deviceIndex
            << PacketIdField(packet)
            << " bytes=" << PacketSize(packet)
            << " tx_power_w=" << txPowerW
            << " sim_time_s=" << Simulator::Now().GetSeconds());
    }

    static void TxEnd(std::string role,
                      uint32_t nodeId,
                      uint32_t deviceIndex,
                      Ptr<const Packet> packet)
    {
        NS_LOG_UNCOND(
            "[PHY_AUDIT] event=PHY_TX_END"
            << " role=" << role
            << " node=" << nodeId
            << " device=" << deviceIndex
            << PacketIdField(packet)
            << " bytes=" << PacketSize(packet)
            << " sim_time_s=" << Simulator::Now().GetSeconds());
    }

    static void RxBegin(std::string role,
                        uint32_t nodeId,
                        uint32_t deviceIndex,
                        Ptr<const Packet> packet,
                        RxPowerWattPerChannelBand rxPowersW)
    {
        NS_LOG_UNCOND(
            "[PHY_AUDIT] event=PHY_RX_BEGIN"
            << " role=" << role
            << " node=" << nodeId
            << " device=" << deviceIndex
            << PacketIdField(packet)
            << " bytes=" << PacketSize(packet)
            << " rx_bands=" << rxPowersW.size()
            << " sim_time_s=" << Simulator::Now().GetSeconds());
    }

    static void RxEnd(std::string role,
                      uint32_t nodeId,
                      uint32_t deviceIndex,
                      Ptr<const Packet> packet)
    {
        NS_LOG_UNCOND(
            "[PHY_AUDIT] event=PHY_RX_END"
            << " role=" << role
            << " node=" << nodeId
            << " device=" << deviceIndex
            << PacketIdField(packet)
            << " bytes=" << PacketSize(packet)
            << " sim_time_s=" << Simulator::Now().GetSeconds());
    }

    static void RxDrop(std::string role,
                       uint32_t nodeId,
                       uint32_t deviceIndex,
                       Ptr<const Packet> packet,
                       WifiPhyRxfailureReason reason)
    {
        NS_LOG_UNCOND(
            "[PHY_AUDIT] event=PHY_RX_DROP"
            << " role=" << role
            << " node=" << nodeId
            << " device=" << deviceIndex
            << PacketIdField(packet)
            << " bytes=" << PacketSize(packet)
            << " reason_code=" << static_cast<uint32_t>(reason)
            << " sim_time_s=" << Simulator::Now().GetSeconds());
    }
};

} // namespace ns3
