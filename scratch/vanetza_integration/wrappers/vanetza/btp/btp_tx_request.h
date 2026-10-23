#pragma once

#include <cstdint>
#include <optional>

namespace ns3
{

enum class BtpTransportType : uint8_t
{
    SHB = 0,
    GBC
};

enum class BtpCommunicationProfile : uint8_t
{
    ITS_G5 = 0
};

struct BtpGeoArea
{
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double radiusMeters = 0.0;
};

struct BtpTxRequest
{
    uint16_t destinationPort = 0;
    uint16_t destinationPortInfo = 0;

    BtpTransportType transport =
        BtpTransportType::SHB;

    BtpCommunicationProfile communicationProfile =
        BtpCommunicationProfile::ITS_G5;

    uint32_t itsAid = 0;

    uint8_t trafficClass = 0;

    // Required for GBC, absent for SHB.
    std::optional<BtpGeoArea> destinationArea;

    // Optional GeoNetworking overrides. Unset values preserve the MIB
    // defaults. Lifetime is expressed in milliseconds at this boundary.
    std::optional<uint32_t> maximumLifetimeMs;
    std::optional<unsigned> maximumHopLimit;
};

} // namespace ns3
