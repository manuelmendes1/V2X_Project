#pragma once

#include <functional>

#include "vanetza_integration/core/phy_audit/phy_audit.h"

#include "ns3/callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mac48-address.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/vector.h"
#include "ns3/wifi-module.h"

namespace ns3
{

struct ItsStationNetworkResources
{
    Ptr<Node> node;
    Ptr<Socket> socket;
    Ipv4Address ipv4Address = Ipv4Address::GetAny();
    Mac48Address linkMac;
};

// The caller owns the station-specific mobility policy. The network builder
// only verifies that a model was installed and applies the initial position.
struct ItsStationNetworkConfig
{
    Vector initialPosition;
    PhyAuditRole phyAuditRole = PhyAuditRole::Rsu;
    std::function<void(Ptr<Node>)> mobilityInstaller;
};

class ItsStationNetworkBuilder
{
public:
    static ItsStationNetworkResources Build(
        Ptr<YansWifiChannel> channel,
        Ipv4AddressHelper& ipv4,
        const Callback<void, Ptr<Socket>>& rxCallback,
        const ItsStationNetworkConfig& config);

private:
    static void ConfigureNetworking(
        Ptr<Node> node,
        Ptr<YansWifiChannel> channel,
        Ipv4AddressHelper& ipv4,
        const ItsStationNetworkConfig& config,
        ItsStationNetworkResources& resources);

    static Ptr<Socket> CreateSocket(
        Ptr<Node> node,
        const Callback<void, Ptr<Socket>>& rxCallback);

    static Mac48Address ResolveLinkMac(Ptr<Node> node);
};

} // namespace ns3
