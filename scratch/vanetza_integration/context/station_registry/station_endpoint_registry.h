#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include <vanetza/geonet/station_type.hpp>

#include "ns3/ipv4-address.h"
#include "ns3/mac48-address.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"

namespace ns3
{

enum class StationEndpointKind : uint8_t
{
    Vehicle,
    ItsStation
};

struct StationEndpointIdentity
{
    StationEndpointKind kind = StationEndpointKind::Vehicle;

    std::string logicalId;
    std::string vehicleId;

    uint32_t nodeId = 0;
    uint32_t stationId = 0;

    vanetza::geonet::StationType stationType =
        vanetza::geonet::StationType::Unknown;

    Ipv4Address ipv4Address = Ipv4Address::GetAny();
    Mac48Address linkMac;
    bool linkMacInitialized = false;
};

inline const char*
StationEndpointKindName(StationEndpointKind kind)
{
    switch (kind)
    {
    case StationEndpointKind::Vehicle:
        return "VEHICLE";

    case StationEndpointKind::ItsStation:
        return "ITS_STATION";
    }

    return "UNKNOWN";
}

class StationEndpointRegistry
{
public:
    static StationEndpointRegistry&
    Instance()
    {
        static StationEndpointRegistry registry;
        return registry;
    }

    const StationEndpointIdentity*
    GetByIpv4(const Ipv4Address& address) const
    {
        const auto it = m_byIpv4.find(address);

        if (it == m_byIpv4.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    bool
    RegisterVehicle(
        const std::string& vehicleId,
        uint32_t nodeId,
        uint32_t stationId,
        vanetza::geonet::StationType stationType,
        const Ipv4Address& ipv4Address,
        const Mac48Address& linkMac,
        bool linkMacInitialized)
    {
        if (vehicleId.empty() ||
            stationId == 0 ||
            stationType == vanetza::geonet::StationType::Unknown ||
            ipv4Address == Ipv4Address::GetAny() ||
            !linkMacInitialized)
        {
            return false;
        }

        const ItsStationIdentityOwner owner{
            ItsStationIdentityOwnerKind::Vehicle,
            vehicleId
        };

        if (!ItsStationIdentityAuthority::Instance().IsReservedBy(
                stationId, owner))
        {
            return false;
        }

        StationEndpointIdentity identity;
        identity.kind = StationEndpointKind::Vehicle;
        identity.logicalId = vehicleId;
        identity.vehicleId = vehicleId;
        identity.nodeId = nodeId;
        identity.stationId = stationId;
        identity.stationType = stationType;
        identity.ipv4Address = ipv4Address;
        identity.linkMac = linkMac;
        identity.linkMacInitialized = true;

        return Register(std::move(identity));
    }

    bool
    RegisterItsStation(
        const std::string& logicalId,
        uint32_t nodeId,
        uint32_t stationId,
        vanetza::geonet::StationType stationType,
        const Ipv4Address& ipv4Address,
        const Mac48Address& linkMac)
    {
        if (logicalId.empty() ||
            stationId == 0 ||
            stationType == vanetza::geonet::StationType::Unknown ||
            ipv4Address == Ipv4Address::GetAny())
        {
            return false;
        }

        const ItsStationIdentityOwner owner{
            ItsStationIdentityOwnerKind::ItsStation,
            logicalId
        };

        if (!ItsStationIdentityAuthority::Instance().IsReservedBy(
                stationId, owner))
        {
            return false;
        }

        StationEndpointIdentity identity;
        identity.kind = StationEndpointKind::ItsStation;
        identity.logicalId = logicalId;
        identity.vehicleId.clear();
        identity.nodeId = nodeId;
        identity.stationId = stationId;
        identity.stationType = stationType;
        identity.ipv4Address = ipv4Address;
        identity.linkMac = linkMac;
        identity.linkMacInitialized = true;

        return Register(std::move(identity));
    }

    void
    UnregisterItsStation(
        const Ipv4Address& address,
        const std::string& logicalId,
        uint32_t stationId)
    {
        const auto it = m_byIpv4.find(address);

        if (it == m_byIpv4.end())
        {
            return;
        }

        const StationEndpointIdentity& registered = it->second;

        if (registered.kind == StationEndpointKind::ItsStation &&
            registered.logicalId == logicalId &&
            registered.stationId == stationId)
        {
            m_byIpv4.erase(it);
        }
    }

    void
    Clear()
    {
        m_byIpv4.clear();
    }

private:
    bool
    Register(StationEndpointIdentity identity)
    {
        const auto existing = m_byIpv4.find(identity.ipv4Address);

        if (existing != m_byIpv4.end())
        {
            const StationEndpointIdentity& current = existing->second;

            const bool sameEndpoint =
                current.kind == identity.kind &&
                current.logicalId == identity.logicalId &&
                current.stationId == identity.stationId;

            if (!sameEndpoint)
            {
                return false;
            }

            existing->second = std::move(identity);
            return true;
        }

        m_byIpv4.emplace(
            identity.ipv4Address,
            std::move(identity));

        return true;
    }

    std::unordered_map<Ipv4Address, StationEndpointIdentity> m_byIpv4;
};

} // namespace ns3
