#pragma once

#include "ns3/callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mac48-address.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/wifi-module.h"

namespace ns3
{

struct ItsStationNetworkResources
{
    Ptr<Node> node;
    Ptr<Socket> socket;

    Ipv4Address ipv4Address =
        Ipv4Address::GetAny();

    Mac48Address linkMac;
};

class ItsStationNetworkBuilder
{
public:

    static ItsStationNetworkResources Build(
        Ptr<YansWifiChannel> channel,
        Ipv4AddressHelper& ipv4,
        const Callback<void, Ptr<Socket>>& rxCallback);

private:

    static void ConfigureNetworking(
        Ptr<Node> node,
        Ptr<YansWifiChannel> channel,
        Ipv4AddressHelper& ipv4,
        ItsStationNetworkResources& resources);

    static Ptr<Socket> CreateSocket(
        Ptr<Node> node,
        const Callback<void, Ptr<Socket>>& rxCallback);

    static Mac48Address ResolveLinkMac(
        Ptr<Node> node);
};

} // namespace ns3
