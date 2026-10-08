#pragma once

#include "its_station_state.h"

#include <vanetza/geonet/address.hpp>

namespace ns3
{

class ItsStationAddressBuilder
{
public:
    static bool Build(
        const ItsStationState& state,
        vanetza::geonet::Address& address);
};

} // namespace ns3
