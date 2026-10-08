#include "its_station_network_builder.h"

#include "ns3/inet-socket-address.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-socket-factory.h"

namespace ns3
{

ItsStationNetworkResources
ItsStationNetworkBuilder::Build(
    Ptr<YansWifiChannel> channel,
    Ipv4AddressHelper& ipv4,
    const Callback<void, Ptr<Socket>>& rxCallback)
{
    NS_ABORT_MSG_IF(
        channel == nullptr,
        "[ITS_STATION_NET] channel is null");

    auto node =
        CreateObject<Node>();

    ItsStationNetworkResources resources{};
    resources.node = node;

    ConfigureNetworking(
        node,
        channel,
        ipv4,
        resources);

    resources.socket =
        CreateSocket(
            node,
            rxCallback);

    NS_ABORT_MSG_IF(
        resources.socket == nullptr,
        "[ITS_STATION_NET] socket creation failed");

    return resources;
}

void
ItsStationNetworkBuilder::ConfigureNetworking(
    Ptr<Node> node,
    Ptr<YansWifiChannel> channel,
    Ipv4AddressHelper& ipv4,
    ItsStationNetworkResources& resources)
{
    NS_ABORT_MSG_IF(
        node == nullptr,
        "[ITS_STATION_NET] node is null");

    InternetStackHelper internet;
    internet.Install(node);

    YansWifiPhyHelper phy;
    phy.SetChannel(channel);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);

    wifi.SetRemoteStationManager(
        "ns3::ConstantRateWifiManager",
        "DataMode",
        StringValue("OfdmRate6MbpsBW10MHz"),
        "ControlMode",
        StringValue("OfdmRate6MbpsBW10MHz"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices =
        wifi.Install(
            phy,
            mac,
            node);

    NS_ABORT_MSG_IF(
        devices.GetN() == 0,
        "[ITS_STATION_NET] no WifiNetDevice created");

    TrafficControlHelper tch;
    tch.SetRootQueueDisc(
        "ns3::PfifoFastQueueDisc");

    tch.Install(devices);

    const Ipv4InterfaceContainer interfaces =
        ipv4.Assign(devices);

    NS_ABORT_MSG_IF(
        interfaces.GetN() == 0,
        "[ITS_STATION_NET] no IPv4 interface assigned");

    resources.ipv4Address =
        interfaces.GetAddress(0);

    resources.linkMac =
        ResolveLinkMac(node);
}

Ptr<Socket>
ItsStationNetworkBuilder::CreateSocket(
    Ptr<Node> node,
    const Callback<void, Ptr<Socket>>& rxCallback)
{
    NS_ABORT_MSG_IF(
        node == nullptr,
        "[ITS_STATION_NET] cannot create socket: node is null");

    Ptr<Socket> socket =
        Socket::CreateSocket(
            node,
            UdpSocketFactory::GetTypeId());

    NS_ABORT_MSG_IF(
        socket == nullptr,
        "[ITS_STATION_NET] UDP socket creation failed");

    socket->SetAllowBroadcast(true);

    const int result =
        socket->Bind(
            InetSocketAddress(
                Ipv4Address::GetAny(),
                5000));

    NS_ABORT_MSG_IF(
        result != 0,
        "[ITS_STATION_NET] failed to bind UDP port 5000");

    if (!rxCallback.IsNull())
    {
        socket->SetRecvCallback(
            rxCallback);
    }

    return socket;
}

Mac48Address
ItsStationNetworkBuilder::ResolveLinkMac(
    Ptr<Node> node)
{
    NS_ABORT_MSG_IF(
        node == nullptr,
        "[ITS_STATION_NET] node is null");

    for (uint32_t i = 0;
         i < node->GetNDevices();
         ++i)
    {
        Ptr<WifiNetDevice> wifiDevice =
            DynamicCast<WifiNetDevice>(
                node->GetDevice(i));

        if (wifiDevice == nullptr)
        {
            continue;
        }

        const Address address =
            wifiDevice->GetAddress();

        NS_ABORT_MSG_IF(
            !Mac48Address::IsMatchingType(address),
            "[ITS_STATION_NET] Wifi device does not expose Mac48Address");

        return Mac48Address::ConvertFrom(address);
    }

    NS_ABORT_MSG(
        "[ITS_STATION_NET] no WifiNetDevice found");

    return Mac48Address();
}

} // namespace ns3
