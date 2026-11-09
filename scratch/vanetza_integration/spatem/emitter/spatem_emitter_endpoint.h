#pragma once

#include <cstdint>
#include <memory>
#include <limits>
#include <optional>
#include <string>

#include <vanetza/geonet/station_type.hpp>

#include "vanetza_integration/context/its_station/network/its_station_network_builder.h"
#include "vanetza_integration/context/its_station/state/its_station_state.h"

namespace ns3
{

struct SpatemEmitterConfig
{
    std::string logicalId;

    uint32_t stationId = 0;

    vanetza::geonet::StationType stationType =
        vanetza::geonet::StationType::RSU;

    std::string trafficLightId;

    uint16_t intersectionId = 0;
    double latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double longitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double ns3X = 0.0;
    double ns3Y = 0.0;
    uint8_t revision = 0;
    uint16_t statusBits = 0;

    std::optional<uint16_t> region;
};

class SpatemEmitterEndpoint
{
public:
    static std::unique_ptr<SpatemEmitterEndpoint>
    Create(const SpatemEmitterConfig& config);

    ~SpatemEmitterEndpoint();

    const std::string& LogicalId() const;

    uint32_t StationId() const;

    vanetza::geonet::StationType StationType() const;

    const std::string& TrafficLightId() const;

    uint16_t IntersectionId() const;

    uint8_t Revision() const;

    uint16_t StatusBits() const;

    const std::optional<uint16_t>& Region() const;

    bool IsVehicle() const;

    ItsStationState& StationState();
    const ItsStationState& StationState() const;

    double LatitudeDeg() const;
    double LongitudeDeg() const;

    bool InitializeNetwork(
        Ptr<YansWifiChannel> channel,
        Ipv4AddressHelper& ipv4,
        const Callback<void, Ptr<Socket>>& rxCallback);

    Ptr<ns3::Node> GetNode() const;

    Ptr<ns3::Socket> GetSocket() const;

    ns3::Ipv4Address GetIpv4Address() const;

    ns3::Mac48Address GetLinkMac() const;

private:
    explicit SpatemEmitterEndpoint(
        const SpatemEmitterConfig& config);

    static bool Validate(
        const SpatemEmitterConfig& config);

private:
    SpatemEmitterConfig m_config;

    ItsStationState m_stationState;
    bool m_stationRegistryRegistered{false};
};

} // namespace ns3
