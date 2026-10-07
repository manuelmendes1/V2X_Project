#pragma once

#include <cstdint>

#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

constexpr uint16_t kSpatemBtpPort = 2004;
constexpr uint32_t kSpatemItsAid = 137;

inline BtpTxRequest
MakeSpatemBtpRequest()
{
    BtpTxRequest request{};

    request.destinationPort =
        kSpatemBtpPort;

    request.destinationPortInfo =
        0;

    request.transport =
        BtpTransportType::SHB;

    request.communicationProfile =
        BtpCommunicationProfile::ITS_G5;

    request.itsAid =
        kSpatemItsAid;

    request.trafficClass =
        0;

    request.destinationArea.reset();

    return request;
}

} // namespace ns3
