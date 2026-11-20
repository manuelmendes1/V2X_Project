#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <vanetza/geonet/station_type.hpp>

#include "vanetza_integration/context/its_station/state/its_station_state.h"
#include "vanetza_integration/vam/types/vam_identity_types.h"

#include "ns3/callback.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/vector.h"
#include "ns3/wifi-module.h"

namespace ns3
{

// Runtime configuration for one SUMO person represented as an ITS-S VRU.
// The station identity is explicit and is independent of SUMO IDs, Node IDs,
// IPv4 addresses and link-layer MAC addresses.
struct VamVruEmitterConfig
{
    std::string personId;
    uint32_t stationId = 0;
    vanetza_integration::vam::VamPersonPresenceEpoch presenceEpoch{};
    Vector initialPosition;
    Vector initialVelocity;
};

// Owns the ns-3 network endpoint for one logical VAM pedestrian station.
// It deliberately does not create VehicleState or register in VehicleRegistry.
class VamVruEmitterEndpoint
{
public:
    static std::unique_ptr<VamVruEmitterEndpoint> Create(
        const VamVruEmitterConfig& config);

    ~VamVruEmitterEndpoint();

    VamVruEmitterEndpoint(const VamVruEmitterEndpoint&) = delete;
    VamVruEmitterEndpoint& operator=(const VamVruEmitterEndpoint&) = delete;
    VamVruEmitterEndpoint(VamVruEmitterEndpoint&&) = delete;
    VamVruEmitterEndpoint& operator=(VamVruEmitterEndpoint&&) = delete;

    const std::string& PersonId() const noexcept;
    const std::string& LogicalId() const noexcept;
    uint32_t StationId() const noexcept;
    vanetza_integration::vam::VamPersonPresenceEpoch
    PresenceEpoch() const noexcept;
    vanetza::geonet::StationType StationType() const noexcept;
    bool IsVehicle() const noexcept;

    ItsStationState& StationState() noexcept;
    const ItsStationState& StationState() const noexcept;

    Ptr<Node> GetNode() const noexcept;
    Ptr<Socket> GetSocket() const noexcept;
    Ipv4Address GetIpv4Address() const noexcept;
    Mac48Address GetLinkMac() const noexcept;

    bool InitializeNetwork(
        Ptr<YansWifiChannel> channel,
        Ipv4AddressHelper& ipv4,
        const Callback<void, Ptr<Socket>>& rxCallback);

    // Applies a complete mobility sample. Invalid input or an uninitialized
    // endpoint is rejected without changing the last valid mobility state.
    bool UpdateMobility(
        const Vector& position,
        const Vector& velocity);

private:
    explicit VamVruEmitterEndpoint(const VamVruEmitterConfig& config);

    static bool Validate(const VamVruEmitterConfig& config) noexcept;
    static std::string MakeLogicalId(const std::string& personId);

    VamVruEmitterConfig m_config;
    ItsStationState m_stationState;
    bool m_stationRegistryRegistered{false};
};

} // namespace ns3
