#include "vanetza_integration/context/its_station/address/its_station_address_builder.h"

#include <vanetza/common/bit_number.hpp>
#include <vanetza/net/mac_address.hpp>

namespace ns3
{

bool
ItsStationAddressBuilder::Build(
    const ItsStationIdentity& identity,
    vanetza::geonet::Address& address)
{
    if (identity.stationId == 0 ||
        identity.stationType == vanetza::geonet::StationType::Unknown)
    {
        return false;
    }

    // Validate before mutating the caller's address.
    vanetza::MacAddress mac =
        vanetza::create_mac_address(identity.stationId);

    // Preserve the deterministic MID policy used by the active vehicle path.
    mac.octets[0] = 0x02;
    mac.octets[1] = 0x00;

    address.mid(mac);
    address.station_type(identity.stationType);
    address.is_manually_configured(true);
    address.country_code(vanetza::BitNumber<unsigned, 10>(0));
    return true;
}

} // namespace ns3
