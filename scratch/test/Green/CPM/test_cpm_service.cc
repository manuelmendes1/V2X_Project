#include "vanetza_integration/cpm/service/cpm_service.h"

#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << name << '\n';
    return condition;
}

CpmInput MakeValidInput()
{
    CpmInput input{};
    input.station.stationId = 9001;
    input.station.origin = CpmRoadSideUnitStationData{};
    input.referenceTimeItsMs = 123456;
    input.referencePosition.latitudeDeg = 41.42528995;
    input.referencePosition.longitudeDeg = -8.43407620;
    input.referencePosition.altitudeMeters = std::nullopt;
    input.knownObjectCount = 1;

    CpmPerceivedObject object{};
    object.objectId = 42;
    object.measurementDeltaTimeMs = -12;
    object.position.xEast = {5.12, std::nullopt};
    object.position.yNorth = {-3.24, std::nullopt};
    object.position.zUp = std::nullopt;
    object.objectAgeMs = 1500;

    input.perceivedObjects.push_back(object);
    return input;
}

CpmInput MakeInputWithObjectCount(std::size_t objectCount)
{
    CpmInput input = MakeValidInput();
    const CpmPerceivedObject templateObject =
        input.perceivedObjects.front();

    input.perceivedObjects.clear();
    input.knownObjectCount = objectCount;

    for (std::size_t i = 0; i < objectCount; ++i)
    {
        CpmPerceivedObject object = templateObject;
        object.objectId = static_cast<std::uint16_t>(i + 1);
        input.perceivedObjects.push_back(object);
    }

    return input;
}

std::shared_ptr<ns3::PacketContext> MakeContext()
{
    auto context = std::make_shared<ns3::PacketContext>();
    context->packetId = 81;
    context->senderNodeId = 7;
    context->senderStationId = 9001;
    context->timestamp = 123456;
    return context;
}

} // namespace

int main()
{
    using namespace ns3;

    bool passed = true;
    unsigned callbackCalls = 0;
    bool sameContext = false;
    std::optional<BtpTxRequest> capturedRequest;
    std::vector<uint8_t> capturedPayload;

    const auto input = MakeValidInput();
    const auto context = MakeContext();

    CpmTransportGateway gateway(
        [&](PacketContext& receivedContext,
            const BtpTxRequest& request,
            const std::vector<uint8_t>& payload)
        {
            ++callbackCalls;
            sameContext = &receivedContext == context.get();
            capturedRequest = request;
            capturedPayload = payload;
            return true;
        });

    const bool sent =
        CpmService::Send(context, input, gateway, 3);

    passed &= Check(sent, "VALID_CPM_SEND_ACCEPTED");
    passed &= Check(callbackCalls == 1, "GATEWAY_CALLED_ONCE");
    passed &= Check(sameContext, "PACKET_CONTEXT_PRESERVED");

    passed &= Check(
        context->hasTxBtpMetadata &&
            context->txBtpDestinationPort == 2009 &&
            context->txItsAid == 639 &&
            context->txGnUpperProtocol == "BTP_B" &&
            context->txGnTransportType == "SHB",
        "TX_METADATA_AVAILABLE_FROM_GENERATION");

    passed &= Check(
        !capturedPayload.empty(),
        "UPER_PAYLOAD_PRODUCED");

    passed &= Check(
        context->payload.bytes == capturedPayload,
        "PACKET_CONTEXT_CONTAINS_TRANSMITTED_BYTES");

    passed &= Check(
        context->payload.type == PayloadType::CPM,
        "PACKET_PAYLOAD_TYPE_IS_CPM");

    passed &= Check(
        capturedRequest.has_value() &&
            capturedRequest->destinationPort == 2009 &&
            capturedRequest->itsAid == 639,
        "CPM_TRANSPORT_POLICY_USED");

    passed &= Check(
        TxBridgeContext::Current() == context,
        "SUCCESSFUL_TX_CONTEXT_RETAINED");

    // Simulate the downstream access completion that owns Pop().
    TxBridgeContext::Pop();

    // The ASN.1 station ID must match the PacketContext sender identity.
    auto mismatchedInput = MakeValidInput();
    mismatchedInput.station.stationId = 9002;
    auto mismatchedContext = MakeContext();
    unsigned callsBeforeMismatch = callbackCalls;

    const bool mismatchSent =
        CpmService::Send(
            mismatchedContext, mismatchedInput, gateway, 3);

    passed &= Check(
        !mismatchSent &&
            mismatchedContext->pipelineResult == PipelineResult::FAIL,
        "SENDER_STATION_ID_MISMATCH_REJECTED");

    passed &= Check(
        callbackCalls == callsBeforeMismatch &&
            TxBridgeContext::Current() == nullptr,
        "MISMATCH_NOT_DISPATCHED");

    // A non-finite coordinate must be rejected by the actual codec path.
    auto invalidInput = MakeValidInput();
    invalidInput.perceivedObjects.front().position.xEast.value =
        std::numeric_limits<double>::quiet_NaN();

    auto invalidContext = MakeContext();
    const unsigned callsBeforeInvalid = callbackCalls;

    const bool invalidSent =
        CpmService::Send(invalidContext, invalidInput, gateway, 3);

    passed &= Check(
        !invalidSent &&
            invalidContext->pipelineResult == PipelineResult::FAIL,
        "INVALID_INPUT_REJECTED_BY_CODEC");

    passed &= Check(
        callbackCalls == callsBeforeInvalid &&
            TxBridgeContext::Current() == nullptr,
        "ENCODE_FAILURE_NOT_DISPATCHED");

    // Um payload acima do limite UPER local tem de falhar na fase
    // ENCODE, antes da invocação do gateway e sem deixar bytes no contexto.
    const auto oversizedInput = MakeInputWithObjectCount(255);
    auto oversizedContext = MakeContext();
    const unsigned callsBeforeOversized = callbackCalls;

    const bool oversizedSent = CpmService::Send(
        oversizedContext, oversizedInput, gateway, 3);

    passed &= Check(
        !oversizedSent &&
            oversizedContext->pipelineResult == PipelineResult::FAIL &&
            oversizedContext->reason ==
                "CPM UPER payload exceeds local maximum of 1200 bytes",
        "OVERSIZED_CPM_REJECTED_BEFORE_TRANSPORT");

    passed &= Check(
        callbackCalls == callsBeforeOversized &&
            oversizedContext->payload.bytes.empty() &&
            TxBridgeContext::Current() == nullptr,
        "OVERSIZED_CPM_NOT_DISPATCHED_OR_RETAINED");

    auto failedTransportContext = MakeContext();

    CpmTransportGateway failedGateway(
        [](PacketContext&,
           const BtpTxRequest&,
           const std::vector<uint8_t>&)
        {
            return false;
        });

    const bool failedSend =
        CpmService::Send(
            failedTransportContext,
            input,
            failedGateway,
            3);

    passed &= Check(
        !failedSend &&
            failedTransportContext->pipelineResult == PipelineResult::FAIL,
        "TRANSPORT_FAILURE_PROPAGATED");

    passed &= Check(
        TxBridgeContext::Current() == nullptr,
        "FAILED_TX_CONTEXT_RELEASED");

    const unsigned callsBeforeNull = callbackCalls;
    const bool nullSend =
        CpmService::Send(nullptr, input, gateway, 3);

    passed &= Check(
        !nullSend && callbackCalls == callsBeforeNull,
        "NULL_CONTEXT_REJECTED");

    if (!passed)
    {
        std::cout << "CPM_SERVICE=FAIL\n";
        return 1;
    }

    std::cout << "CPM_SERVICE=PASS\n";
    return 0;
}
