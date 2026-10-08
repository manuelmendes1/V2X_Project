#include "vanetza_integration/spatem/types/spatem_input.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"
#include "vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_decoder.h"
#include "vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_encoder.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace
{

SpatemInput MakeValidInput()
{
    SpatemInput input;
    input.stationId = 9001;

    SpatemIntersection intersection;
    intersection.intersectionId = 42;
    intersection.revision = 7;

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

int Fail(const std::string& message)
{
    std::cerr << "FAIL " << message << std::endl;
    return 1;
}

int Pass(const std::string& message)
{
    std::cout << "PASS " << message << std::endl;
    return 0;
}

bool Encodes(const SpatemInput& input)
{
    return VanetzaSpatemCodec::Encode(input).success;
}

} // namespace

int main()
{
    // ============================================================
    // 1. Timing absent remains valid
    // ============================================================

    {
        const auto input = MakeValidInput();

        const auto encoded =
            VanetzaSpatemCodec::Encode(input);

        if (!encoded.success)
        {
            return Fail(
                "SPATEM without TimeChangeDetails remains valid");
        }

        const auto decoded =
            VanetzaSpatemCodec::Decode(encoded.bytes);

        if (!decoded.success)
        {
            return Fail(
                "SPATEM without TimeChangeDetails decodes");
        }

        if (!decoded.message.intersections.front()
                 .movements.front()
                 .events.front()
                 .timing.has_value())
        {
            Pass("absence of TimeChangeDetails survives round-trip");
        }
        else
        {
            return Fail(
                "absent timing unexpectedly became present");
        }
    }

    // ============================================================
    // 2. TimeMark lower boundary
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.startTime = 0;
        timing.minEndTime = 0;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (!Encodes(input))
        {
            return Fail("TimeMark value 0 is accepted");
        }

        Pass("TimeMark value 0 is accepted");
    }

    // ============================================================
    // 3. TimeMark upper boundary
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.startTime = 36001;
        timing.minEndTime = 36001;
        timing.maxEndTime = 36001;
        timing.likelyTime = 36001;
        timing.nextTime = 36001;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (!Encodes(input))
        {
            return Fail("TimeMark value 36001 is accepted");
        }

        Pass("TimeMark value 36001 is accepted");
    }

    // ============================================================
    // 4. TimeMark upper boundary + 1 rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.minEndTime = 36002;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (Encodes(input))
        {
            return Fail(
                "TimeMark value 36002 is rejected");
        }

        Pass("TimeMark value 36002 is rejected");
    }

    // ============================================================
    // 5. Confidence lower and upper boundaries
    // ============================================================

    {
        for (uint8_t confidence : {uint8_t{0}, uint8_t{15}})
        {
            auto input = MakeValidInput();

            SpatemTiming timing;
            timing.minEndTime = 1000;
            timing.confidence = confidence;

            input.intersections.front()
                .movements.front()
                .events.front()
                .timing = timing;

            if (!Encodes(input))
            {
                return Fail(
                    "confidence boundary " +
                    std::to_string(confidence) +
                    " is accepted");
            }

            Pass(
                "confidence boundary " +
                std::to_string(confidence) +
                " is accepted");
        }
    }

    // ============================================================
    // 6. Confidence upper boundary + 1 rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.minEndTime = 1000;
        timing.confidence = 16;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (Encodes(input))
        {
            return Fail(
                "confidence value 16 is rejected");
        }

        Pass("confidence value 16 is rejected");
    }

    // ============================================================
    // 7. Reserved status bits rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        input.intersections.front().statusBits = 0x4000;

        if (Encodes(input))
        {
            return Fail(
                "status reserved bit 14 is rejected");
        }

        Pass("status reserved bit 14 is rejected");
    }

    // ============================================================
    // 8. Invalid phase state rejected
    //
    // Cast deliberately to exercise the builder boundary.
    // ============================================================

    {
        auto input = MakeValidInput();

        input.intersections.front()
            .movements.front()
            .events.front()
            .phaseState =
                static_cast<SpatemMovementPhaseState>(10);

        if (Encodes(input))
        {
            return Fail(
                "MovementPhaseState value 10 is rejected");
        }

        Pass("MovementPhaseState value 10 is rejected");
    }

    // ============================================================
    // 9. Empty movement list rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        input.intersections.front().movements.clear();

        if (Encodes(input))
        {
            return Fail("empty MovementList is rejected");
        }

        Pass("empty MovementList is rejected");
    }

    // ============================================================
    // 10. Empty event list rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        input.intersections.front()
            .movements.front()
            .events.clear();

        if (Encodes(input))
        {
            return Fail("empty MovementEventList is rejected");
        }

        Pass("empty MovementEventList is rejected");
    }

    // ============================================================
    // 11. Empty intersection list rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        input.intersections.clear();

        if (Encodes(input))
        {
            return Fail(
                "empty IntersectionStateList is rejected");
        }

        Pass(
            "empty IntersectionStateList is rejected");
    }

    // ============================================================
    // 12. MsgCount upper boundary + 1 rejected
    // ============================================================

    {
        auto input = MakeValidInput();
        input.intersections.front().revision = 128;

        if (Encodes(input))
        {
            return Fail(
                "MsgCount value 128 is rejected");
        }

        Pass("MsgCount value 128 is rejected");
    }

    // ============================================================
    // 13. DSecond reserved range rejected
    // ============================================================

    {
        auto input = MakeValidInput();
        input.intersections.front().dSecond = 61000;

        if (Encodes(input))
        {
            return Fail(
                "DSecond reserved value 61000 is rejected");
        }

        Pass("DSecond reserved value 61000 is rejected");
    }

    // ============================================================
    // 14. DSecond reserved upper value rejected
    // ============================================================

    {
        auto input = MakeValidInput();
        input.intersections.front().dSecond = 65534;

        if (Encodes(input))
        {
            return Fail(
                "DSecond reserved value 65534 is rejected");
        }

        Pass("DSecond reserved value 65534 is rejected");
    }

    // ============================================================
    // 15. DSecond unavailable value accepted
    // ============================================================

    {
        auto input = MakeValidInput();
        input.intersections.front().dSecond = 65535;

        if (!Encodes(input))
        {
            return Fail(
                "DSecond unavailable value 65535 is accepted");
        }

        Pass("DSecond unavailable value 65535 is accepted");
    }

    // ============================================================
    // 16. MinuteOfTheYear invalid sentinel rejected
    // ============================================================

    {
        auto input = MakeValidInput();
        input.intersections.front().minuteOfYear = 527040;

        if (Encodes(input))
        {
            return Fail(
                "MinuteOfTheYear invalid sentinel 527040 is rejected");
        }

        Pass("MinuteOfTheYear invalid sentinel 527040 is rejected");
    }

    // ============================================================
    // 17. likelyTime beyond maxEndTime rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.minEndTime = 100;
        timing.maxEndTime = 200;
        timing.likelyTime = 300;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (Encodes(input))
        {
            return Fail(
                "likelyTime beyond maxEndTime is rejected");
        }

        Pass("likelyTime beyond maxEndTime is rejected");
    }

        // ============================================================
    // 18. MinuteOfTheYear last valid value accepted
    // ============================================================

    {
        auto input = MakeValidInput();
        input.intersections.front().minuteOfYear = 527039;

        if (!Encodes(input))
        {
            return Fail(
                "MinuteOfTheYear value 527039 is accepted");
        }

        Pass("MinuteOfTheYear value 527039 is accepted");
    }

    // ============================================================
    // 19. likelyTime below minEndTime rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.minEndTime = 100;
        timing.maxEndTime = 200;
        timing.likelyTime = 50;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (Encodes(input))
        {
            return Fail(
                "likelyTime below minEndTime is rejected");
        }

        Pass("likelyTime below minEndTime is rejected");
    }

    // ============================================================
    // 20. maxEndTime below minEndTime rejected
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.minEndTime = 200;
        timing.maxEndTime = 100;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (Encodes(input))
        {
            return Fail(
                "maxEndTime below minEndTime is rejected");
        }

        Pass("maxEndTime below minEndTime is rejected");
    }

    // ============================================================
    // 21. likelyTime equal to both boundaries accepted
    // ============================================================

    {
        auto input = MakeValidInput();

        SpatemTiming timing;
        timing.minEndTime = 100;
        timing.maxEndTime = 100;
        timing.likelyTime = 100;

        input.intersections.front()
            .movements.front()
            .events.front()
            .timing = timing;

        if (!Encodes(input))
        {
            return Fail(
                "likelyTime equal to minEndTime and maxEndTime is accepted");
        }

        Pass(
            "likelyTime equal to minEndTime and maxEndTime is accepted");
    }


    // ============================================================
    // 22. Invalid intersection identifier rejected
    //
    // intersectionId == 0 is semantically invalid for SPATEM.
    // This case validates the implemented builder
    // implements the same validation already enforced by the
    // SpatemEmitterEndpoint boundary.
    // ============================================================

    {
        auto input = MakeValidInput();

        input.intersections.front().intersectionId = 0;

        if (Encodes(input))
        {
            return Fail(
                "intersectionId 0 is rejected");
        }

        Pass("intersectionId 0 is rejected");
    }


    // ============================================================
    // 23. Wrong SPATEM message ID rejected by codec
    //
    // Build a valid SPATEM, mutate only the ASN.1 ITS PDU
    // messageID from SPATEM to CAM, re-encode it, and verify
    // the codec rejects it specifically at the messageID check.
    // ============================================================

    {
        const auto valid =
            VanetzaSpatemCodec::Encode(
                MakeValidInput());

        if (!valid.success)
        {
            return Fail(
                "valid SPATEM encodes before wrong messageID test");
        }

        auto raw =
            SpatemUperDecoder::Decode(
                valid.bytes);

        if (!raw.success ||
            !raw.spatem)
        {
            return Fail(
                "valid SPATEM decodes before wrong messageID test");
        }

        raw.spatem->header.messageID =
            ItsPduHeader__messageID_cam;

        const auto mutated =
            SpatemUperEncoder::Encode(
                *raw.spatem);

        if (!mutated.success)
        {
            return Fail(
                "SPATEM with wrong messageID re-encodes");
        }

        const auto decoded =
            VanetzaSpatemCodec::Decode(
                mutated.bytes);

        if (decoded.success)
        {
            return Fail(
                "wrong SPATEM messageID is rejected");
        }

        if (decoded.error !=
            "invalid SPATEM messageID")
        {
            return Fail(
                "wrong SPATEM messageID reports the expected error");
        }

        Pass(
            "wrong SPATEM messageID is rejected");
    }


    std::cout
        << "SPATEM_CODEC_BOUNDARY_TEST_EXIT=0"
        << std::endl;

    return 0;
}
