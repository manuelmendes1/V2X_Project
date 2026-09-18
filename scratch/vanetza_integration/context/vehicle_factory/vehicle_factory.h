#pragma once

#include <string>

#include "vanetza_integration/context/vehicle_network/vehicle_network_builder.h"

#include "ns3/callback.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/wifi-module.h"

namespace ns3
{

class VehicleFactory
{
public:

    static void Initialize(
        Ptr<YansWifiChannel> channel,
        const Ipv4AddressHelper& ipv4,
        const Callback<void, Ptr<Socket>>& rxCallback);

    static Ptr<Node> CreateVehicle(
        const std::string& vehId);

private:

    static void ConfigureMobility(
        Ptr<Node> node);
};

} // namespace ns3
