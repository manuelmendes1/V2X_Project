#include "spatem_emitter_endpoint.h"

namespace ns3
{

std::unique_ptr<SpatemEmitterEndpoint>
SpatemEmitterEndpoint::Create(
    const SpatemEmitterConfig& config)
{
    if (!Validate(config))
    {
        return nullptr;
    }

    return std::unique_ptr<SpatemEmitterEndpoint>(
        new SpatemEmitterEndpoint(config));
}

SpatemEmitterEndpoint::SpatemEmitterEndpoint(
    const SpatemEmitterConfig& config)
    : m_config(config)
{
    m_stationState.identity.logicalId =
        config.logicalId;

    m_stationState.identity.stationId =
        config.stationId;

    m_stationState.identity.stationType =
        config.stationType;
}

bool
SpatemEmitterEndpoint::Validate(
    const SpatemEmitterConfig& config)
{
    if (config.logicalId.empty())
    {
        return false;
    }

    if (config.stationId == 0)
    {
        return false;
    }

    if (config.stationType !=
        vanetza::geonet::StationType::RSU)
    {
        return false;
    }

    if (config.trafficLightId.empty())
    {
        return false;
    }

    if (config.intersectionId == 0)
    {
        return false;
    }

    if (config.statusBits & 0xC000U)
    {
        return false;
    }

    return true;
}

const std::string&
SpatemEmitterEndpoint::LogicalId() const
{
    return m_config.logicalId;
}

uint32_t
SpatemEmitterEndpoint::StationId() const
{
    return m_config.stationId;
}

vanetza::geonet::StationType
SpatemEmitterEndpoint::StationType() const
{
    return m_config.stationType;
}

const std::string&
SpatemEmitterEndpoint::TrafficLightId() const
{
    return m_config.trafficLightId;
}

uint16_t
SpatemEmitterEndpoint::IntersectionId() const
{
    return m_config.intersectionId;
}

uint8_t
SpatemEmitterEndpoint::Revision() const
{
    return m_config.revision;
}

uint16_t
SpatemEmitterEndpoint::StatusBits() const
{
    return m_config.statusBits;
}

const std::optional<uint16_t>&
SpatemEmitterEndpoint::Region() const
{
    return m_config.region;
}

bool
SpatemEmitterEndpoint::IsVehicle() const
{
    return false;
}

const ItsStationState&
SpatemEmitterEndpoint::StationState() const
{
    return m_stationState;
}

bool
SpatemEmitterEndpoint::InitializeNetwork(
    Ptr<YansWifiChannel> channel,
    Ipv4AddressHelper& ipv4,
    const Callback<void, Ptr<Socket>>& rxCallback)
{
    if (m_stationState.networkInitialized)
    {
        return true;
    }

    const ItsStationNetworkResources resources =
        ItsStationNetworkBuilder::Build(
            channel,
            ipv4,
            rxCallback);

    if (resources.node == nullptr ||
        resources.socket == nullptr ||
        resources.ipv4Address ==
            ns3::Ipv4Address::GetAny())
    {
        return false;
    }

    m_stationState.node =
        resources.node;

    m_stationState.socket =
        resources.socket;

    m_stationState.ipv4Address =
        resources.ipv4Address;

    m_stationState.linkMac =
        resources.linkMac;

    m_stationState.networkInitialized = true;

    return true;
}

Ptr<Node>
SpatemEmitterEndpoint::GetNode() const
{
    return m_stationState.node;
}

Ptr<Socket>
SpatemEmitterEndpoint::GetSocket() const
{
    return m_stationState.socket;
}

Ipv4Address
SpatemEmitterEndpoint::GetIpv4Address() const
{
    return m_stationState.ipv4Address;
}

Mac48Address
SpatemEmitterEndpoint::GetLinkMac() const
{
    return m_stationState.linkMac;
}

} // namespace ns3
