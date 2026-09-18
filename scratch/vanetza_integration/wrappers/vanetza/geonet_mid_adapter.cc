#include "geonet_mid_adapter.h"

namespace ns3
{

GeoNetMid
VanetzaGeoNetMidAdapter::ToGeoNetMid(
    const vanetza::MacAddress& mid)
{
    GeoNetMid result{};

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        result[i] = mid.octets[i];
    }

    return result;
}

} // namespace ns3
