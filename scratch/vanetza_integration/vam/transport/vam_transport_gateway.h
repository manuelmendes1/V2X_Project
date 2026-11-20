#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "vanetza_integration/context/tx_bridge/tx_completion.h"
#include "vanetza_integration/vam/transport/vam_transport_policy.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_metadata.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3
{

class VamTransportGateway
{
public:
    using SendFunction =
        std::function<bool(
            PacketContext&,
            const BtpTxRequest&,
            const std::vector<uint8_t>&)>;

    using AsyncSendFunction =
        std::function<bool(
            PacketContext&,
            const BtpTxRequest&,
            const std::vector<uint8_t>&,
            std::shared_ptr<TxCompletion>)>;

    // Preserve the original synchronous gateway API.
    explicit VamTransportGateway(SendFunction sendFunction)
        : m_sendFunction(std::move(sendFunction))
    {
    }

    // Select the asynchronous constructor only for a four-argument callback.
    // A constrained template also keeps VamTransportGateway({}) unambiguous.
    template <typename Function>
        requires std::is_invocable_r_v<
            bool,
            Function&,
            PacketContext&,
            const BtpTxRequest&,
            const std::vector<uint8_t>&,
            std::shared_ptr<TxCompletion>>
    explicit VamTransportGateway(Function&& sendFunction)
        : m_asyncSendFunction(std::forward<Function>(sendFunction))
    {
    }

    bool Send(
        PacketContext& context,
        const std::vector<uint8_t>& payload,
        uint8_t trafficClass) const
    {
        const BtpTxRequest request =
            MakeVamBtpRequest(trafficClass);

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

    // true transfers responsibility for exactly-once terminal completion to
    // the transport callback. It may complete inline or retain the shared
    // completion handle and complete it later. false leaves resolution with
    // the caller; this method deliberately does not synthesize a result.
    bool Submit(
        PacketContext& context,
        const std::vector<uint8_t>& payload,
        uint8_t trafficClass,
        std::shared_ptr<TxCompletion> completion) const
    {
        const BtpTxRequest request =
            MakeVamBtpRequest(trafficClass);

        return Submit(
            context,
            request,
            payload,
            std::move(completion));
    }

    bool Submit(
        PacketContext& context,
        const BtpTxRequest& request,
        const std::vector<uint8_t>& payload,
        std::shared_ptr<TxCompletion> completion) const
    {
        if (!m_asyncSendFunction || payload.empty() || !completion)
        {
            return false;
        }

        SetPacketContextTxBtpMetadata(context, request);

        return m_asyncSendFunction(
            context,
            request,
            payload,
            std::move(completion));
    }

private:
    SendFunction m_sendFunction;
    AsyncSendFunction m_asyncSendFunction;
};

} // namespace ns3
