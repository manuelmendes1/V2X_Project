#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_metadata.h"
#include "vanetza_integration/cpm/transport/cpm_transport_policy.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

class CpmTransportGateway
{
public:
    using SendFunction =
        std::function<bool(
            PacketContext&,
            const BtpTxRequest&,
            const std::vector<uint8_t>&)>;

    explicit CpmTransportGateway(SendFunction sendFunction)
        : m_sendFunction(std::move(sendFunction))
    {
    }

    bool Send(
        PacketContext& context,
        const std::vector<uint8_t>& payload,
        uint8_t trafficClass) const
    {
        const BtpTxRequest request =
            MakeCpmBtpRequest(trafficClass);

        return Send(context, request, payload);
    }

    bool Send(
        PacketContext& context,
        const BtpTxRequest& request,
        const std::vector<uint8_t>& payload) const
    {
        if (!m_sendFunction || payload.empty())
        {
            return false;
        }

        SetPacketContextTxBtpMetadata(context, request);

        return m_sendFunction(
            context,
            request,
            payload);
    }

private:
    SendFunction m_sendFunction;
};

} // namespace ns3
