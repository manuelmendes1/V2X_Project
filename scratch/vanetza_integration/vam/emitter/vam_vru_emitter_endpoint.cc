#include "vanetza_integration/vam/emitter/vam_vru_emitter_endpoint.h"

#include <cmath>
#include <utility>

#include "vanetza_integration/context/its_station/network/its_station_network_builder.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"

#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"

namespace ns3
{
namespace
{

bool IsFiniteVector(const Vector& value) noexcept
{
    return std::isfinite(value.x) &&
           std::isfinite(value.y) &&
           std::isfinite(value.z);
}

} // namespace

std::unique_ptr<VamVruEmitterEndpoint>
VamVruEmitterEndpoint::Create(const VamVruEmitterConfig& config)
{
    if (!Validate(config))
    {
        return nullptr;
    }

    const ItsStationIdentityOwner owner{
        ItsStationIdentityOwnerKind::ItsStation,
        MakeLogicalId(config.personId)};

    // Reserve is intentionally idempotent for a scenario-pre-reserved
    // (logical owner, stationId) pair. It rejects both owner changes and
    // stationId collisions with any other ITS station or vehicle.
    if (!ItsStationIdentityAuthority::Instance().Reserve(
            owner, config.stationId))
    {
        NS_LOG_UNCOND(
            "[VAM_VRU_ENDPOINT] ERROR global stationId reservation failed"
            << " person_id=" << config.personId
            << " logical_id=" << owner.logicalId
            << " station_id=" << config.stationId);
        return nullptr;
    }

    return std::unique_ptr<VamVruEmitterEndpoint>(
        new VamVruEmitterEndpoint(config));
}

VamVruEmitterEndpoint::VamVruEmitterEndpoint(
    const VamVruEmitterConfig& config)
    : m_config(config)
{
    m_stationState.identity.logicalId = MakeLogicalId(config.personId);
    m_stationState.identity.stationId = config.stationId;
    m_stationState.identity.stationType =
        vanetza::geonet::StationType::Pedestrian;
}

VamVruEmitterEndpoint::~VamVruEmitterEndpoint()
{
    if (m_stationRegistryRegistered)
    {
        StationEndpointRegistry::Instance().UnregisterItsStation(
            m_stationState.ipv4Address,
            m_stationState.identity.logicalId,
            m_stationState.identity.stationId);
        m_stationRegistryRegistered = false;
    }

    // Do not release ItsStationIdentityAuthority here. The reservation is
    // logical-person state and must survive SUMO REMOVE/re-entry cycles.
}

bool
VamVruEmitterEndpoint::Validate(
    const VamVruEmitterConfig& config) noexcept
{
    return !config.personId.empty() &&
           config.stationId != 0 &&
           IsFiniteVector(config.initialPosition) &&
           IsFiniteVector(config.initialVelocity);
}

std::string
VamVruEmitterEndpoint::MakeLogicalId(const std::string& personId)
{
    return "vam-vru:" + personId;
}

const std::string&
VamVruEmitterEndpoint::PersonId() const noexcept
{
    return m_config.personId;
}

const std::string&
VamVruEmitterEndpoint::LogicalId() const noexcept
{
    return m_stationState.identity.logicalId;
}

uint32_t
VamVruEmitterEndpoint::StationId() const noexcept
{
    return m_stationState.identity.stationId;
}

vanetza_integration::vam::VamPersonPresenceEpoch
VamVruEmitterEndpoint::PresenceEpoch() const noexcept
{
    return m_config.presenceEpoch;
}

vanetza::geonet::StationType
VamVruEmitterEndpoint::StationType() const noexcept
{
    return m_stationState.identity.stationType;
}

bool
VamVruEmitterEndpoint::IsVehicle() const noexcept
{
    return false;
}

ItsStationState&
VamVruEmitterEndpoint::StationState() noexcept
{
    return m_stationState;
}

const ItsStationState&
VamVruEmitterEndpoint::StationState() const noexcept
{
    return m_stationState;
}

Ptr<Node>
VamVruEmitterEndpoint::GetNode() const noexcept
{
    return m_stationState.node;
}

Ptr<Socket>
VamVruEmitterEndpoint::GetSocket() const noexcept
{
    return m_stationState.socket;
}

Ipv4Address
VamVruEmitterEndpoint::GetIpv4Address() const noexcept
{
    return m_stationState.ipv4Address;
}

Mac48Address
VamVruEmitterEndpoint::GetLinkMac() const noexcept
{
    return m_stationState.linkMac;
}

bool
VamVruEmitterEndpoint::InitializeNetwork(
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
        NS_LOG_UNCOND(
            "[VAM_VRU_ENDPOINT] ERROR Wi-Fi channel is null"
            << " logical_id=" << LogicalId());
        return false;
    }

    ItsStationNetworkConfig networkConfig;
    networkConfig.initialPosition = m_config.initialPosition;
    networkConfig.phyAuditRole = PhyAuditRole::Vru;
    const Vector initialVelocity = m_config.initialVelocity;
    networkConfig.mobilityInstaller = [initialVelocity](Ptr<Node> node)
    {
        MobilityHelper mobility;
        mobility.SetMobilityModel(
            "ns3::ConstantVelocityMobilityModel");
        mobility.Install(node);

        Ptr<ConstantVelocityMobilityModel> model =
            node->GetObject<ConstantVelocityMobilityModel>();
        NS_ABORT_MSG_IF(
            model == nullptr,
            "[VAM_VRU_ENDPOINT] ConstantVelocityMobilityModel installation failed");
        model->SetVelocity(initialVelocity);
    };

    const ItsStationNetworkResources resources =
        ItsStationNetworkBuilder::Build(
            channel,
            ipv4,
            rxCallback,
            networkConfig);

    if (resources.node == nullptr ||
        resources.socket == nullptr ||
        resources.ipv4Address == Ipv4Address::GetAny())
    {
        NS_LOG_UNCOND(
            "[VAM_VRU_ENDPOINT] ERROR network initialization failed"
            << " logical_id=" << LogicalId());
        return false;
    }

    const bool registered =
        StationEndpointRegistry::Instance().RegisterItsStation(
            m_stationState.identity.logicalId,
            resources.node->GetId(),
            m_stationState.identity.stationId,
            m_stationState.identity.stationType,
            resources.ipv4Address,
            resources.linkMac);

    if (!registered)
    {
        NS_LOG_UNCOND(
            "[VAM_VRU_ENDPOINT] ERROR station registry rejected endpoint"
            << " logical_id=" << LogicalId()
            << " station_id=" << StationId()
            << " ipv4=" << resources.ipv4Address);
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

bool
VamVruEmitterEndpoint::UpdateMobility(
    const Vector& position,
    const Vector& velocity)
{
    if (!m_stationState.networkInitialized ||
        m_stationState.node == nullptr ||
        !IsFiniteVector(position) ||
        !IsFiniteVector(velocity))
    {
        return false;
    }

    Ptr<ConstantVelocityMobilityModel> mobility =
        m_stationState.node->GetObject<ConstantVelocityMobilityModel>();
    if (mobility == nullptr)
    {
        return false;
    }

    // SetPosition() in ns-3.47 may clear ConstantVelocity mobility. Apply
    // velocity afterwards so the submitted sample remains coherent.
    mobility->SetPosition(position);
    mobility->SetVelocity(velocity);
    return true;
}

} // namespace ns3
