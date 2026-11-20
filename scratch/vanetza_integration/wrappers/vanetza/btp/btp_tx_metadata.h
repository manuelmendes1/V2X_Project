#pragma once

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

inline void
SetPacketContextTxBtpMetadata(
    PacketContext& context,
    const BtpTxRequest& request)
{
    context.hasTxBtpMetadata = true;
    context.txBtpDestinationPort = request.destinationPort;
    context.txItsAid = request.itsAid;
    context.txGnUpperProtocol = "BTP_B";
    context.txGnTransportType = BtpTransportName(request.transport);
}

} // namespace ns3
