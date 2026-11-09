#include "vanetza_integration/cpm/emitter/cpm_emitter_endpoint.h"

#include <cmath>

#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"
#include "vanetza_integration/context/its_station/network/its_station_network_builder.h"

#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/vector.h"

#include <vanetza/geonet/station_type.hpp>

namespace ns3
{

std::unique_ptr<CpmEmitterEndpoint>
CpmEmitterEndpoint::Create(const CpmEmitterConfig& config)
{
    if (!Validate(config))
    {
        return nullptr;
    }

    return std::unique_ptr<CpmEmitterEndpoint>(
        new CpmEmitterEndpoint(config));
}

CpmEmitterEndpoint::CpmEmitterEndpoint(
    const CpmEmitterConfig& config)
    : m_config(config)
{
    m_stationState.identity.logicalId = config.logicalId;
    m_stationState.identity.stationId = config.stationId;
    m_stationState.identity.stationType =
        vanetza::geonet::StationType::RSU;
}

CpmEmitterEndpoint::~CpmEmitterEndpoint()
{
    if (m_stationRegistryRegistered)
    {
        StationEndpointRegistry::Instance().UnregisterItsStation(
            m_stationState.ipv4Address,
            m_stationState.identity.logicalId,
            m_stationState.identity.stationId);
    }
}

bool
CpmEmitterEndpoint::Validate(const CpmEmitterConfig& config)
{
    return !config.logicalId.empty() &&
        config.stationId != 0 &&
        std::isfinite(config.latitudeDeg) &&
        config.latitudeDeg >= -90.0 &&
        config.latitudeDeg <= 90.0 &&
        std::isfinite(config.longitudeDeg) &&
        config.longitudeDeg >= -180.0 &&
        config.longitudeDeg <= 180.0 &&
        std::isfinite(config.ns3X) &&
        std::isfinite(config.ns3Y);
}

bool
CpmEmitterEndpoint::InitializeNetwork(
    Ptr<YansWifiChannel> channel,
    Ipv4AddressHelper& ipv4,
    const Callback<void, Ptr<Socket>>& rxCallback)
{
    if (m_stationState.networkInitialized)
    {
        return true;
    }

    if (channel == nullptr)
    {
        NS_LOG_UNCOND("[CPM_RSU] ERROR Wi-Fi channel is null");
        return false;
    }
    ItsStationNetworkConfig networkConfig;
    networkConfig.initialPosition = Vector(m_config.ns3X, m_config.ns3Y, 0.0);
    networkConfig.phyAuditRole = PhyAuditRole::Rsu;
    networkConfig.mobilityInstaller = [](Ptr<Node> node)
    {
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(node);
    };

    const auto resources = ItsStationNetworkBuilder::Build(
        channel,
        ipv4,
        rxCallback,
        networkConfig);

    if (resources.node == nullptr ||
        resources.socket == nullptr ||
        resources.ipv4Address == Ipv4Address::GetAny())
    {
        NS_LOG_UNCOND("[CPM_RSU] ERROR RSU network initialization failed");
        return false;
    }

    const bool registered =
        StationEndpointRegistry::Instance().RegisterItsStation(
            m_config.logicalId,
            resources.node->GetId(),
            m_config.stationId,
            vanetza::geonet::StationType::RSU,
            resources.ipv4Address,
            resources.linkMac);

    if (!registered)
    {
        NS_LOG_UNCOND(
            "[CPM_RSU] ERROR station registry rejected endpoint "
            << m_config.logicalId);
        return false;
    }

    m_stationState.node = resources.node;
    m_stationState.socket = resources.socket;
    m_stationState.ipv4Address = resources.ipv4Address;
    m_stationState.linkMac = resources.linkMac;
    m_stationState.networkInitialized = true;
    m_stationRegistryRegistered = true;

    return true;
}

const std::string&
CpmEmitterEndpoint::LogicalId() const
{
    return m_config.logicalId;
}

uint32_t
CpmEmitterEndpoint::StationId() const
{
    return m_config.stationId;
}

double
CpmEmitterEndpoint::LatitudeDeg() const
{
    return m_config.latitudeDeg;
}

double
CpmEmitterEndpoint::LongitudeDeg() const
{
    return m_config.longitudeDeg;
}

ItsStationState&
CpmEmitterEndpoint::StationState()
{
    return m_stationState;
}

const ItsStationState&
CpmEmitterEndpoint::StationState() const
{
    return m_stationState;
}

Ptr<Node>
CpmEmitterEndpoint::GetNode() const
{
    return m_stationState.node;
}

Ptr<Socket>
CpmEmitterEndpoint::GetSocket() const
{
    return m_stationState.socket;
}

Ipv4Address
CpmEmitterEndpoint::GetIpv4Address() const
{
    return m_stationState.ipv4Address;
}

Mac48Address
CpmEmitterEndpoint::GetLinkMac() const
{
    return m_stationState.linkMac;
}

} // namespace ns3
