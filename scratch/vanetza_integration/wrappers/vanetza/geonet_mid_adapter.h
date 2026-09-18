#pragma once

#include <array>
#include <cstdint>

#include <vanetza/net/mac_address.hpp>

#include "vanetza_integration/context/geo_net/geo_net_mid.h"

namespace ns3
{

class VanetzaGeoNetMidAdapter
{
public:

    static GeoNetMid ToGeoNetMid(
        const vanetza::MacAddress& mid);
};

} // namespace ns3
