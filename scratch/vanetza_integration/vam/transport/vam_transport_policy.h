#pragma once

#include <cstdint>

#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

// ETSI VRU Awareness Basic Service: BTP destination port.
constexpr uint16_t kVamBtpPort = 2018;

// ITS-AID assigned to VRU Awareness.
constexpr uint32_t kVamItsAid = 638;

// VAM direct communication profile: maximum packet lifetime <= 1000 ms.
constexpr uint32_t kVamMaximumPacketLifetimeMs = 1000;

inline BtpTxRequest
MakeVamBtpRequest(uint8_t trafficClass)
{
    BtpTxRequest request{};

    request.destinationPort = kVamBtpPort;
    request.destinationPortInfo = 0;
    request.transport = BtpTransportType::SHB;
    request.communicationProfile = BtpCommunicationProfile::ITS_G5;
    request.itsAid = kVamItsAid;
    request.trafficClass = trafficClass;
    request.maximumLifetimeMs = kVamMaximumPacketLifetimeMs;

    // Preserve the configured GeoNetworking MIB hop-limit default.
    request.maximumHopLimit.reset();

    // SHB does not carry a destination area.
    request.destinationArea.reset();

    return request;
}

} // namespace ns3
