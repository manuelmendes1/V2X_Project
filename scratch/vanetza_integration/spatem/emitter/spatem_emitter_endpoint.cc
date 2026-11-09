#include "spatem_emitter_endpoint.h"

#include <cmath>
#include <cstdlib>
#include "ns3/mobility-module.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"

namespace ns3
{

SpatemEmitterEndpoint::~SpatemEmitterEndpoint()
{
    if (m_stationRegistryRegistered)
    {
        StationEndpointRegistry::Instance().UnregisterItsStation(
            m_stationState.ipv4Address,
            m_stationState.identity.logicalId,
            m_stationState.identity.stationId);
        m_stationRegistryRegistered = false;
    }
}


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

    if (!std::isfinite(config.latitudeDeg) ||
        config.latitudeDeg < -90.0 ||
        config.latitudeDeg > 90.0 ||
        !std::isfinite(config.longitudeDeg) ||
        config.longitudeDeg < -180.0 ||
        config.longitudeDeg > 180.0)
    {
        return false;
    }

    if (!std::isfinite(config.ns3X) ||
        !std::isfinite(config.ns3Y))
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

ItsStationState&
SpatemEmitterEndpoint::StationState()
{
    return m_stationState;
}

const ItsStationState&
SpatemEmitterEndpoint::StationState() const
{
    return m_stationState;
}

double
SpatemEmitterEndpoint::LatitudeDeg() const
{
    return m_config.latitudeDeg;
}

double
SpatemEmitterEndpoint::LongitudeDeg() const
{
    return m_config.longitudeDeg;
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
    ItsStationNetworkConfig networkConfig;
    networkConfig.initialPosition = Vector(m_config.ns3X, m_config.ns3Y, 0.0);
    networkConfig.phyAuditRole = PhyAuditRole::Rsu;
    networkConfig.mobilityInstaller = [](Ptr<Node> node)
    {
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(node);
    };

    const ItsStationNetworkResources resources =
        ItsStationNetworkBuilder::Build(
            channel,
            ipv4,
            rxCallback,
            networkConfig);

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

    const char* const unknownSenderTest =
        std::getenv("V2X_TEST_UNKNOWN_SPATEM_SENDER");
    const bool leaveRsuUnregistered =
        unknownSenderTest != nullptr &&
        unknownSenderTest[0] == '1' &&
        unknownSenderTest[1] == '\0';

    if (leaveRsuUnregistered)
    {
        NS_LOG_UNCOND(
            "[TEST_UNKNOWN_SENDER] RSU endpoint intentionally unregistered"
            << " source_ip=" << m_stationState.ipv4Address
            << " station=" << m_stationState.identity.stationId);
    }
    else
    {
        const bool registered =
            StationEndpointRegistry::Instance().RegisterItsStation(
                m_stationState.identity.logicalId,
                m_stationState.node->GetId(),
                m_stationState.identity.stationId,
                m_stationState.identity.stationType,
                m_stationState.ipv4Address,
                m_stationState.linkMac);

        NS_ABORT_MSG_IF(
            !registered,
            "[STATION_REGISTRY] Failed to register ITS station IPv4 endpoint: "
                << m_stationState.identity.logicalId);

        m_stationRegistryRegistered = true;
    }

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
