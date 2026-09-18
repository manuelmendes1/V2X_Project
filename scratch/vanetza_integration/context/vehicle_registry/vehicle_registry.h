
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "../vehicle_state.h"

#include "ns3/mac48-address.h"

#include "../geo_net/geo_net_mid.h"

namespace ns3 {

class VehicleRegistry
{
public:

    // =====================================================================
    // Singleton Access
    // =====================================================================

    static VehicleRegistry& Instance();

    // =====================================================================
    // Vehicle lookup / ownership
    // =====================================================================

    // Creates a VehicleState if vehId is not registered yet, otherwise
    // returns the existing VehicleState.
    //
    // VehicleRegistry owns VehicleState instances.
    // VehicleRegistry does NOT generate or redefine vehicle identity.

    VehicleState* CreateOrGet(
        int64_t step,
        const std::string& vehId);

    VehicleState* GetByVehId(
        const std::string& vehId);

    VehicleState* GetByNodeId(
        uint32_t nodeId);

    VehicleState* GetByStationId(
        uint32_t stationId);

    // =====================================================================
    // Identity indexes
    //
    // VehicleIdentity is authoritative.
    //
    // These functions ONLY create/update reverse-lookup indexes:
    //
    //     nodeId    -> vehId
    //     stationId -> vehId
    //
    // They MUST NOT modify:
    //
    //     VehicleState::identity.vehId
    //     VehicleState::identity.nodeId
    //     VehicleState::identity.stationId
    //
    // Identity is initialized by VehicleIdentityInitializer.
    // =====================================================================

    void RegisterNodeId(
        int64_t step,
        const std::string& vehId,
        uint32_t nodeId);

    void RegisterStationId(
        int64_t step,
        const std::string& vehId,
        uint32_t stationId);

    // =====================================================================
    // Link-layer address index
    //
    // linkMac is the REAL ns-3 WifiNetDevice MAC address.
    //
    // It belongs to the Link Layer and is independent from:
    //
    //     stationId
    //     GN_ADDR
    //     GN_ADDR.MID
    //
    // This index MUST contain only ns-3 Link MAC addresses.
    // It MUST NOT contain a GeoNetworking MID.
    // =====================================================================

    VehicleState* GetByLinkMac(
        const Mac48Address& mac);

    void RegisterLinkMac(
        const Mac48Address& mac,
        VehicleState& vehicle);

    // =====================================================================
    // GeoNetworking identity index
    //
    // GN_ADDR.MID -> VehicleState
    //
    // The MID represents the ETSI GeoNetworking identity carried by
    // GN_ADDR.
    //
    // It is derived deterministically from VehicleIdentity::stationId
    // by VanetzaRuntime.
    //
    // This index MUST contain only GeoNetworking MID values.
    // It MUST NOT contain the ns-3 Wifi Link MAC.
    // =====================================================================

    VehicleState* GetByMid(
        const GeoNetMid& mid);

    void RegisterMid(
        const GeoNetMid& mid,
        VehicleState& vehicle);

    // =====================================================================
    // IPv4 index
    // =====================================================================

    VehicleState* GetByIpv4(
        const Ipv4Address& address);

    void RegisterIpv4(
        const Ipv4Address& address,
        VehicleState& vehicle);

    // =====================================================================
    // Helpers
    // =====================================================================

    uint32_t GetStationId(
        const std::string& vehId);

    void Clear();

    const std::unordered_map<
        std::string,
        std::unique_ptr<VehicleState>>&
    GetAllVehicles() const;

    bool Exists(
        const std::string& vehId) const;

    bool Empty() const;

    std::size_t Size() const;

private:

    VehicleRegistry() = default;

    VehicleRegistry(const VehicleRegistry&) = delete;
    VehicleRegistry& operator=(const VehicleRegistry&) = delete;

private:

    // =====================================================================
    // Ownership
    // =====================================================================

    // Single owner of all VehicleState instances.
    std::unordered_map<
        std::string,
        std::unique_ptr<VehicleState>>
    vehiclesById;

    // =====================================================================
    // Reverse identity indexes
    // =====================================================================

    // nodeId -> SUMO vehId
    std::unordered_map<
        uint32_t,
        std::string>
    nodeToVeh;

    // stationId -> SUMO vehId
    std::unordered_map<
        uint32_t,
        std::string>
    stationToVeh;

    // =====================================================================
    // Link-layer index
    // =====================================================================
    //
    // ns-3 Wifi Link MAC -> VehicleState
    //
    // IMPORTANT:
    // This contains only the physical/link-layer MAC obtained from
    // WifiNetDevice::GetAddress().
    //
    // It MUST NOT contain a GeoNetworking MID.
    //

    std::map<
        Mac48Address,
        VehicleState*>
    m_byLinkMac;

    // =====================================================================
    // GeoNetworking identity index
    // =====================================================================
    //
    // GN_ADDR.MID -> VehicleState
    //
    // IMPORTANT:
    // This contains only the ETSI GeoNetworking MID.
    // It is NOT the ns-3 Link MAC.
    //

    std::map<
        GeoNetMid,
        VehicleState*>
    m_byMid;

    // =====================================================================
    // IPv4 index
    // =====================================================================

    std::unordered_map<
        Ipv4Address,
        VehicleState*>
    m_byIpv4;
};

} // namespace ns3
