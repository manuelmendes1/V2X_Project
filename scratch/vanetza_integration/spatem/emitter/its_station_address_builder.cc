#include "its_station_address_builder.h"

#include <vanetza/common/bit_number.hpp>
#include <vanetza/net/mac_address.hpp>

namespace ns3
{

bool
ItsStationAddressBuilder::Build(
    const ItsStationState& state,
    vanetza::geonet::Address& address)
{
    if (state.identity.stationId == 0)
    {
        return false;
    }

    if (state.identity.stationType ==
        vanetza::geonet::StationType::Unknown)
    {
        return false;
    }

    vanetza::MacAddress mac =
        vanetza::create_mac_address(
            state.identity.stationId);

    // Preserve the deterministic MID policy already used
    // by the active Vanetza vehicle path.
    mac.octets[0] = 0x02;
    mac.octets[1] = 0x00;

    address.mid(mac);

    address.station_type(
        state.identity.stationType);

    address.is_manually_configured(true);

    address.country_code(
        vanetza::BitNumber<unsigned, 10>(0));

    return true;
}

} // namespace ns3
