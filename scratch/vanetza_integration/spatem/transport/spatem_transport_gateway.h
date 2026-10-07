#pragma once

#include <functional>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/transport/spatem_transport_policy.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

class SpatemTransportGateway
{
public:

    using SendFunction =
        std::function<bool(
            PacketContext&,
            const BtpTxRequest&,
            const std::vector<uint8_t>&)>;

    explicit SpatemTransportGateway(
        SendFunction sendFunction)
        : m_sendFunction(std::move(sendFunction))
    {
    }

    bool Send(
        PacketContext& context,
        const std::vector<uint8_t>& payload) const
    {
        if (!m_sendFunction)
        {
            return false;
        }

        const BtpTxRequest request =
            MakeSpatemBtpRequest();

        return m_sendFunction(
            context,
            request,
            payload);
    }

private:

    SendFunction m_sendFunction;
};

} // namespace ns3
