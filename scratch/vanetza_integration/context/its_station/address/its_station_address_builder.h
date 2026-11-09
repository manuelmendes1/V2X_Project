#pragma once

#include "vanetza_integration/context/its_station/state/its_station_state.h"

#include <vanetza/geonet/address.hpp>

namespace ns3
{

class ItsStationAddressBuilder
{
public:
    static bool Build(
        const ItsStationIdentity& identity,
        vanetza::geonet::Address& address);

    // Compatibility overload for current endpoint consumers during migration.
    static bool Build(
        const ItsStationState& state,
        vanetza::geonet::Address& address)
    {
        return Build(state.identity, address);
    }
};

} // namespace ns3
