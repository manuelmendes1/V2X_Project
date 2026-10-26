#include "vanetza_integration/cpm/transport/cpm_transport_gateway.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << name << '\n';
    return condition;
}

} // namespace

int main()
{
    using namespace ns3;

    bool passed = true;
    bool transportResult = true;
    unsigned callbackCalls = 0;
    bool sameContext = false;
    std::optional<BtpTxRequest> capturedRequest;
    std::vector<uint8_t> capturedPayload;

    PacketContext context{};
    context.packetId = 81;
    context.senderStationId = 9001;

    const std::vector<uint8_t> encodedPayload{
        0x20, 0x01, 0xCA, 0xFE, 0x35
    };

    CpmTransportGateway gateway(
        [&](PacketContext& receivedContext,
            const BtpTxRequest& request,
            const std::vector<uint8_t>& payload)
        {
            ++callbackCalls;
            sameContext = &receivedContext == &context;
            capturedRequest = request;
            capturedPayload = payload;
            return transportResult;
        });

    const bool sent =
        gateway.Send(context, encodedPayload, 3);

    passed &= Check(sent, "TRANSPORT_RESULT_PROPAGATED");
    passed &= Check(callbackCalls == 1, "CALLBACK_INVOKED_ONCE");
    passed &= Check(sameContext, "PACKET_CONTEXT_PRESERVED");
    passed &= Check(
        capturedPayload == encodedPayload,
        "ENCODED_BYTES_PRESERVED");

    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->destinationPort == 2009,
        "CPM_BTP_PORT_2009");

    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->itsAid == 639,
        "CPM_ITS_AID_639");

    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->transport == BtpTransportType::SHB,
        "CPM_TRANSPORT_SHB");

    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->communicationProfile ==
                BtpCommunicationProfile::ITS_G5,
        "CPM_PROFILE_ITS_G5");

    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->trafficClass == 3,
        "TRAFFIC_CLASS_FORWARDED");

    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->maximumLifetimeMs.has_value() &&
            *capturedRequest->maximumLifetimeMs == 1000,
        "CPM_MAXIMUM_LIFETIME_1000_MS");

    passed &= Check(
        capturedRequest.has_value() &&
            !capturedRequest->maximumHopLimit.has_value(),
        "HOP_LIMIT_REMAINS_MIB_DEFAULT");

    transportResult = false;
    const bool secondSend =
        gateway.Send(context, encodedPayload, 3);

    passed &= Check(
        !secondSend && callbackCalls == 2,
        "TRANSPORT_FAILURE_PROPAGATED");

    const unsigned callsBeforeEmpty = callbackCalls;
    const bool emptySend =
        gateway.Send(context, {}, 3);

    passed &= Check(
        !emptySend && callbackCalls == callsBeforeEmpty,
        "EMPTY_PAYLOAD_NOT_DISPATCHED");

    CpmTransportGateway unavailableGateway({});
    passed &= Check(
        !unavailableGateway.Send(context, encodedPayload, 3),
        "MISSING_TRANSPORT_CALLBACK_REJECTED");

    if (!passed)
    {
        std::cout << "CPM_TRANSPORT_GATEWAY=FAIL\n";
        return 1;
    }

    std::cout << "CPM_TRANSPORT_GATEWAY=PASS\n";
    return 0;
}
