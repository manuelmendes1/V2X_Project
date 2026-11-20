#include "vanetza_integration/vam/transport/vam_transport_gateway.h"
#include "vanetza_integration/context/tx_bridge/tx_completion.h"

#include <cstdint>
#include <iostream>
#include <memory>
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
    using CompletionPtr = std::shared_ptr<TxCompletion>;

    bool passed = true;
    PacketContext context{};
    context.packetId = 63801;
    context.senderStationId = 9001;

    const std::vector<std::uint8_t> payload{0x31, 0x32, 0x33, 0x34};
    unsigned acceptedCalls = 0;
    bool sameContext = false;
    bool sameCompletion = false;
    std::optional<BtpTxRequest> capturedRequest;
    std::vector<std::uint8_t> capturedPayload;
    CompletionPtr transportOwnedCompletion;

    unsigned completionCalls = 0;
    TxCompletionResult observedResult{};
    auto completion = std::make_shared<TxCompletion>(
        [&](const TxCompletionResult& result)
        {
            ++completionCalls;
            observedResult = result;
        });

    VamTransportGateway asynchronousGateway(
        [&](PacketContext& receivedContext,
            const BtpTxRequest& request,
            const std::vector<std::uint8_t>& receivedPayload,
            CompletionPtr receivedCompletion)
        {
            ++acceptedCalls;
            sameContext = &receivedContext == &context;
            sameCompletion = receivedCompletion == completion;
            capturedRequest = request;
            capturedPayload = receivedPayload;
            transportOwnedCompletion = std::move(receivedCompletion);
            return true;
        });

    const bool accepted = asynchronousGateway.Submit(
        context, payload, 7, completion);

    passed &= Check(accepted, "ASYNC_SUBMISSION_ACCEPTED");
    passed &= Check(acceptedCalls == 1, "ASYNC_TRANSPORT_CALLBACK_ONCE");
    passed &= Check(sameContext, "ASYNC_PACKET_CONTEXT_PRESERVED");
    passed &= Check(sameCompletion, "ASYNC_COMPLETION_HANDLE_FORWARDED");
    passed &= Check(
        transportOwnedCompletion == completion,
        "TRANSPORT_RETAINS_COMPLETION_OWNERSHIP");
    passed &= Check(
        !completion->IsCompleted() && completionCalls == 0,
        "ASYNC_SUBMISSION_IS_NOT_TERMINAL_COMPLETION");
    passed &= Check(
        capturedPayload == payload,
        "ASYNC_UPER_PAYLOAD_PRESERVED");
    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->destinationPort == 2018 &&
            capturedRequest->itsAid == 638 &&
            capturedRequest->transport == BtpTransportType::SHB &&
            capturedRequest->communicationProfile ==
                BtpCommunicationProfile::ITS_G5 &&
            capturedRequest->trafficClass == 7 &&
            capturedRequest->maximumLifetimeMs == 1000,
        "ASYNC_VAM_BTP_POLICY_PRESERVED");
    passed &= Check(
        context.hasTxBtpMetadata &&
            context.txBtpDestinationPort == 2018 &&
            context.txItsAid == 638 &&
            context.txGnUpperProtocol == "BTP_B" &&
            context.txGnTransportType == "SHB",
        "ASYNC_PACKET_CONTEXT_BTP_METADATA");

    const bool firstTerminal = transportOwnedCompletion->Complete({
        TxCompletionStatus::Success,
        123456,
        "test_async_terminal_success"
    });
    const bool duplicateTerminal = transportOwnedCompletion->Complete({
        TxCompletionStatus::Failure,
        123457,
        "duplicate_must_be_ignored"
    });

    passed &= Check(firstTerminal, "ASYNC_TERMINAL_CALLBACK_ACCEPTED");
    passed &= Check(!duplicateTerminal, "ASYNC_DUPLICATE_TERMINAL_REJECTED");
    passed &= Check(
        completionCalls == 1 &&
            observedResult.status == TxCompletionStatus::Success &&
            observedResult.completedAtSimUs == 123456 &&
            observedResult.reason == "test_async_terminal_success",
        "ASYNC_COMPLETION_EXACTLY_ONCE");

    // The transport may complete inline, before Submit() returns.
    unsigned synchronousCompletionCalls = 0;
    auto synchronousCompletion = std::make_shared<TxCompletion>(
        [&](const TxCompletionResult& result)
        {
            ++synchronousCompletionCalls;
            if (result.status != TxCompletionStatus::Success)
            {
                ++synchronousCompletionCalls;
            }
        });
    VamTransportGateway synchronousGateway(
        [](PacketContext&,
           const BtpTxRequest&,
           const std::vector<std::uint8_t>&,
           CompletionPtr callbackCompletion)
        {
            callbackCompletion->Complete({
                TxCompletionStatus::Success,
                200000,
                "test_synchronous_terminal_success"
            });
            return true;
        });

    const bool synchronousAccepted = synchronousGateway.Submit(
        context, payload, 4, synchronousCompletion);
    passed &= Check(
        synchronousAccepted && synchronousCompletion->IsCompleted() &&
            synchronousCompletionCalls == 1,
        "SYNCHRONOUS_COMPLETION_BEFORE_SUBMIT_RETURN");

    // A false submission result leaves terminal resolution with the caller.
    unsigned rejectedCalls = 0;
    auto rejectedCompletion = std::make_shared<TxCompletion>();
    VamTransportGateway rejectingGateway(
        [&](PacketContext&,
            const BtpTxRequest&,
            const std::vector<std::uint8_t>&,
            CompletionPtr)
        {
            ++rejectedCalls;
            return false;
        });

    const bool rejected = rejectingGateway.Submit(
        context, payload, 2, rejectedCompletion);
    passed &= Check(!rejected, "SYNC_SUBMISSION_REJECTION_PROPAGATED");
    passed &= Check(rejectedCalls == 1, "REJECTING_TRANSPORT_CALLED_ONCE");
    passed &= Check(
        !rejectedCompletion->IsCompleted(),
        "REJECTED_SUBMISSION_RETAINS_CALLER_RESOLUTION");
    passed &= Check(
        rejectedCompletion->Complete({
            TxCompletionStatus::Failure,
            300000,
            "caller_resolved_synchronous_rejection"
        }),
        "CALLER_CAN_RESOLVE_REJECTED_SUBMISSION");

    const unsigned callsBeforeEmpty = rejectedCalls;
    auto emptyCompletion = std::make_shared<TxCompletion>();
    const bool emptyAccepted = rejectingGateway.Submit(
        context, {}, 2, emptyCompletion);
    passed &= Check(
        !emptyAccepted && rejectedCalls == callsBeforeEmpty &&
            !emptyCompletion->IsCompleted(),
        "EMPTY_PAYLOAD_REJECTED_BEFORE_TRANSPORT");

    const unsigned callsBeforeNullCompletion = rejectedCalls;
    const bool nullCompletionAccepted = rejectingGateway.Submit(
        context, payload, 2, {});
    passed &= Check(
        !nullCompletionAccepted && rejectedCalls == callsBeforeNullCompletion,
        "MISSING_COMPLETION_REJECTED_BEFORE_TRANSPORT");

    VamTransportGateway unavailableGateway({});
    auto unavailableCompletion = std::make_shared<TxCompletion>();
    const bool unavailableAccepted = unavailableGateway.Submit(
        context, payload, 2, unavailableCompletion);
    passed &= Check(
        !unavailableAccepted && !unavailableCompletion->IsCompleted(),
        "MISSING_TRANSPORT_REJECTED_WITH_CALLER_OWNERSHIP");

    if (!passed)
    {
        std::cout << "VAM_ASYNC_TRANSPORT_GATEWAY=FAIL\n";
        return 1;
    }

    std::cout << "VAM_ASYNC_TRANSPORT_GATEWAY=PASS\n";
    return 0;
}
