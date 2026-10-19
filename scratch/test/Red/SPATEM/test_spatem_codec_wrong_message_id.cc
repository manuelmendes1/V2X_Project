#include <iostream>
#include <string>

#include "vanetza_integration/wrappers/vanetza/spatem/spatem_asn1_builder.h"
#include "vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_encoder.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

namespace
{

SpatemInput ValidInput()
{
    SpatemInput input;
    input.stationId = 9001;

    SpatemIntersection intersection;
    intersection.intersectionId = 42;

    SpatemMovement movement;
    movement.signalGroup = 1;

    SpatemMovementEvent event;
    event.phaseState =
        SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;

    movement.events.push_back(event);
    intersection.movements.push_back(movement);
    input.intersections.push_back(intersection);

    return input;
}

bool CheckValidSpatemControl()
{
    const auto asn = SpatemAsn1Builder::Build(ValidInput());

    if (!asn)
    {
        std::cout << "[FAIL] valid_spatem_control reason=builder_failed\n";
        return false;
    }

    const auto encoded = SpatemUperEncoder::Encode(*asn);

    if (!encoded.success)
    {
        std::cout << "[FAIL] valid_spatem_control encode_error=\""
                  << encoded.failedType << "\"\n";
        return false;
    }

    const auto decoded = VanetzaSpatemCodec::Decode(encoded.bytes);

    const bool passed =
        decoded.success &&
        decoded.message.stationId == 9001;

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << "valid_spatem_control"
              << " success=" << decoded.success
              << " stationId=" << decoded.message.stationId;

    if (!decoded.error.empty())
    {
        std::cout << " error=\"" << decoded.error << "\"";
    }

    std::cout << '\n';
    return passed;
}

bool CheckWrongMessageIdRejected()
{
    auto asn = SpatemAsn1Builder::Build(ValidInput());

    if (!asn)
    {
        std::cout << "[FAIL] wrong_message_id_rejected"
                  << " reason=builder_failed\n";
        return false;
    }

    // MAPEM is a valid ItsPduHeader message ID, but is not SPATEM.
    asn->header.messageID = ItsPduHeader__messageID_mapem;

    // Encode with the production UPER encoder. The resulting bytes must
    // be valid ASN.1 before testing the codec's message-ID validation.
    const auto encoded = SpatemUperEncoder::Encode(*asn);

    if (!encoded.success)
    {
        std::cout << "[FAIL] wrong_message_id_rejected"
                  << " reason=test_payload_encoding_failed"
                  << " error=\"" << encoded.failedType << "\"\n";
        return false;
    }

    const auto decoded = VanetzaSpatemCodec::Decode(encoded.bytes);

    const bool passed =
        !decoded.success &&
        decoded.error == "invalid SPATEM messageID";

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << "wrong_message_id_rejected"
              << " success=" << decoded.success
              << " error=\"" << decoded.error << "\"\n";

    return passed;
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    if (CheckValidSpatemControl())
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    if (CheckWrongMessageIdRejected())
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    std::cout << "SPATEM_WRONG_MESSAGE_ID_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_WRONG_MESSAGE_ID_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_WRONG_MESSAGE_ID_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_WRONG_MESSAGE_ID_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
