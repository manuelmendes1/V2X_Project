#pragma once

#include <cstdint>
#include <string>

#include <vanetza/geonet/station_type.hpp>

#include "ns3/ipv4-address.h"
#include "ns3/mac48-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"

namespace ns3
{

// Identity is independent of ns-3 node IDs, IPv4 addresses and link MACs.
struct ItsStationIdentity
{
    std::string logicalId;
    uint32_t stationId = 0;
    vanetza::geonet::StationType stationType =
        vanetza::geonet::StationType::Unknown;
};

// Runtime resources owned by a non-vehicle ITS station endpoint.
struct ItsStationState
{
    ItsStationIdentity identity;
    Ptr<Node> node;
    Ptr<Socket> socket;
    Ipv4Address ipv4Address = Ipv4Address::GetAny();
    Mac48Address linkMac;
    bool networkInitialized = false;
};

} // namespace ns3
