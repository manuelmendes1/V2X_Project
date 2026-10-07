#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/transport/spatem_transport_policy.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

using SpatemRuntimeSendFunction =
    std::function<bool(
        PacketContext&,
        const BtpTxRequest&,
        const std::vector<uint8_t>&)>;

inline bool
SendSpatemViaRuntime(
    const SpatemRuntimeSendFunction& sendFunction,
    PacketContext& context,
    const std::vector<uint8_t>& payload)
{
    if (!sendFunction)
    {
        return false;
    }

    const BtpTxRequest request =
        MakeSpatemBtpRequest();

    return sendFunction(
        context,
        request,
        payload);
}

} // namespace ns3
