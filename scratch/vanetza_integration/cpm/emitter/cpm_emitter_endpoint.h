#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "vanetza_integration/context/its_station/state/its_station_state.h"

#include "ns3/callback.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/wifi-module.h"

namespace ns3
{

struct CpmEmitterConfig
{
    std::string logicalId;
    uint32_t stationId = 0;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double ns3X = 0.0;
    double ns3Y = 0.0;
};

class CpmEmitterEndpoint
{
public:
    static std::unique_ptr<CpmEmitterEndpoint> Create(
        const CpmEmitterConfig& config);

    ~CpmEmitterEndpoint();

    CpmEmitterEndpoint(const CpmEmitterEndpoint&) = delete;
    CpmEmitterEndpoint& operator=(const CpmEmitterEndpoint&) = delete;

    bool InitializeNetwork(
        Ptr<YansWifiChannel> channel,
        Ipv4AddressHelper& ipv4,
        const Callback<void, Ptr<Socket>>& rxCallback);

    const std::string& LogicalId() const;
    uint32_t StationId() const;
    double LatitudeDeg() const;
    double LongitudeDeg() const;
    ItsStationState& StationState();
    const ItsStationState& StationState() const;
    Ptr<Node> GetNode() const;
    Ptr<Socket> GetSocket() const;
    Ipv4Address GetIpv4Address() const;
    Mac48Address GetLinkMac() const;

private:
    explicit CpmEmitterEndpoint(const CpmEmitterConfig& config);
    static bool Validate(const CpmEmitterConfig& config);

    CpmEmitterConfig m_config;
    ItsStationState m_stationState;
    bool m_stationRegistryRegistered{false};
};

} // namespace ns3
