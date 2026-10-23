#pragma once

#include <cstdint>

#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

// ETSI TS 103 248: BTP destination port for CPM.
constexpr uint16_t kCpmBtpPort = 2009;

// ITS-AID registered for the Collective Perception Service.
constexpr uint32_t kCpmItsAid = 639;

// ETSI TS 103 324 V2.1.1: maximum packet lifetime <= 1000 ms.
constexpr uint32_t kCpmMaximumPacketLifetimeMs = 1000;

inline BtpTxRequest
MakeCpmBtpRequest(uint8_t trafficClass)
{
    BtpTxRequest request{};

    request.destinationPort = kCpmBtpPort;
    request.destinationPortInfo = 0;
    request.transport = BtpTransportType::SHB;
    request.communicationProfile = BtpCommunicationProfile::ITS_G5;
    request.itsAid = kCpmItsAid;
    request.trafficClass = trafficClass;
    request.maximumLifetimeMs = kCpmMaximumPacketLifetimeMs;

    // Use the GeoNetworking MIB default unless explicitly configured.
    request.maximumHopLimit.reset();

    // SHB has no destination area.
    request.destinationArea.reset();

    return request;
}

} // namespace ns3
