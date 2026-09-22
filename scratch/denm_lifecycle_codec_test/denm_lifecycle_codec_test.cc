#include "vanetza_integration/wrappers/vanetza/denm/vanetza_denm_codec.h"

#include <cstdint>
#include <iostream>

namespace
{

DenmInput
MakeInput(
    DenmManagementAction action,
    uint32_t stationId,
    uint16_t sequence)
{
    DenmInput input;

    input.action = action;

    input.actionId.originatingStationId =
        stationId;

    input.actionId.sequenceNumber =
        sequence;

    input.detectionTimeItsMs =
        1000;

    input.referenceTimeItsMs =
        1000;

    input.eventPosition.latitude =
        41.15;

    input.eventPosition.longitude =
        -8.61;

    input.informationQuality =
        1;

    input.eventType.causeCode =
        94;

    input.eventType.subCauseCode =
        0;

    input.stationType =
        5;

    return input;
}

bool
Expect(
    bool condition,
    const char* description)
{
    if (condition)
    {
        std::cout
            << "PASS: "
            << description
            << std::endl;

        return true;
    }

    std::cout
        << "FAIL: "
        << description
        << std::endl;

    return false;
}

} // namespace

int main()
{
    bool ok = true;

    constexpr uint32_t stationId =
        42;

    constexpr uint16_t sequenceNumber =
        7;

    // ------------------------------------------------------------
    // NEW
    // ------------------------------------------------------------

    const auto newEncoded =
        VanetzaDenmCodec::Encode(
            MakeInput(
                DenmManagementAction::NEW,
                stationId,
                sequenceNumber));

    ok &= Expect(
        newEncoded.success,
        "NEW encodes successfully");

    const auto newDecoded =
        VanetzaDenmCodec::Decode(
            newEncoded.bytes);

    ok &= Expect(
        newDecoded.success,
        "NEW decodes successfully");

    ok &= Expect(
        newDecoded.message.action ==
            DenmManagementAction::NEW,
        "NEW decodes with no termination");

    ok &= Expect(
        newDecoded.message.actionId.originatingStationId ==
                stationId &&
        newDecoded.message.actionId.sequenceNumber ==
                sequenceNumber,
        "NEW preserves ActionID");

    // ------------------------------------------------------------
    // UPDATE
    //
    // UPDATE intentionally uses the same ASN.1 representation as
    // NEW: same ActionID and no termination.
    //
    // The raw codec therefore decodes it as NEW. The lifecycle
    // layer later classifies it as UPDATE based on existing state
    // plus a different payload.
    // ------------------------------------------------------------

    const auto updateEncoded =
        VanetzaDenmCodec::Encode(
            MakeInput(
                DenmManagementAction::UPDATE,
                stationId,
                sequenceNumber));

    ok &= Expect(
        updateEncoded.success,
        "UPDATE encodes successfully");

    const auto updateDecoded =
        VanetzaDenmCodec::Decode(
            updateEncoded.bytes);

    ok &= Expect(
        updateDecoded.success,
        "UPDATE wire representation decodes successfully");

    ok &= Expect(
        updateDecoded.message.action ==
            DenmManagementAction::NEW,
        "UPDATE has no termination on the wire");

    ok &= Expect(
        updateDecoded.message.actionId.originatingStationId ==
                stationId &&
        updateDecoded.message.actionId.sequenceNumber ==
                sequenceNumber,
        "UPDATE preserves the same ActionID");

    // ------------------------------------------------------------
    // CANCELLATION
    // ------------------------------------------------------------

    const auto cancellationEncoded =
        VanetzaDenmCodec::Encode(
            MakeInput(
                DenmManagementAction::CANCELLATION,
                stationId,
                sequenceNumber));

    ok &= Expect(
        cancellationEncoded.success,
        "CANCELLATION encodes successfully");

    const auto cancellationDecoded =
        VanetzaDenmCodec::Decode(
            cancellationEncoded.bytes);

    ok &= Expect(
        cancellationDecoded.success,
        "CANCELLATION decodes successfully");

    ok &= Expect(
        cancellationDecoded.message.action ==
            DenmManagementAction::CANCELLATION,
        "CANCELLATION decodes from isCancellation");

    // ------------------------------------------------------------
    // NEGATION
    // ------------------------------------------------------------

    const auto negationEncoded =
        VanetzaDenmCodec::Encode(
            MakeInput(
                DenmManagementAction::NEGATION,
                stationId,
                sequenceNumber));

    ok &= Expect(
        negationEncoded.success,
        "NEGATION encodes successfully");

    const auto negationDecoded =
        VanetzaDenmCodec::Decode(
            negationEncoded.bytes);

    ok &= Expect(
        negationDecoded.success,
        "NEGATION decodes successfully");

    ok &= Expect(
        negationDecoded.message.action ==
            DenmManagementAction::NEGATION,
        "NEGATION decodes from isNegation");

    if (!ok)
    {
        std::cout
            << "DENM_CODEC_LIFECYCLE_EXIT=1"
            << std::endl;

        return 1;
    }

    std::cout
        << "DENM_CODEC_LIFECYCLE_EXIT=0"
        << std::endl;

    return 0;
}
