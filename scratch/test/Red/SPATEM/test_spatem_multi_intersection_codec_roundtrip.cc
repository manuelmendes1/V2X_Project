#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

namespace
{

SpatemInput MakeInput()
{
    SpatemInput input;
    input.stationId = 9001;

    SpatemIntersection first;
    first.region = 100;
    first.intersectionId = 42;
    first.revision = 7;
    first.statusBits = 0x0123U;
    first.minuteOfYear = 12345U;
    first.dSecond = 1234U;

    SpatemTiming firstTiming;
    firstTiming.startTime = 100U;
    firstTiming.minEndTime = 200U;
    firstTiming.maxEndTime = 300U;
    firstTiming.likelyTime = 250U;
    firstTiming.confidence = 10U;
    firstTiming.nextTime = 400U;

    SpatemMovementEvent firstEvent;
    firstEvent.phaseState =
        SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;
    firstEvent.timing = firstTiming;

    SpatemMovement firstMovement;
    firstMovement.signalGroup = 3U;
    firstMovement.events.push_back(firstEvent);
    first.movements.push_back(firstMovement);

    SpatemIntersection second;
    second.region = 200;
    second.intersectionId = 43;
    second.revision = 8;
    second.statusBits = 0x0234U;
    second.minuteOfYear = 23456U;
    second.dSecond = 2345U;

    SpatemMovementEvent secondEvent;
    secondEvent.phaseState =
        SpatemMovementPhaseState::PERMISSIVE_MOVEMENT_ALLOWED;
    // This event deliberately has no timing details.

    SpatemMovement secondMovement;
    secondMovement.signalGroup = 9U;
    secondMovement.events.push_back(secondEvent);
    second.movements.push_back(secondMovement);

    input.intersections.push_back(first);
    input.intersections.push_back(second);

    return input;
}

bool SameTiming(
    const SpatemTiming& expected,
    const SpatemTiming& actual)
{
    return expected.startTime == actual.startTime &&
           expected.minEndTime == actual.minEndTime &&
           expected.maxEndTime == actual.maxEndTime &&
           expected.likelyTime == actual.likelyTime &&
           expected.confidence == actual.confidence &&
           expected.nextTime == actual.nextTime;
}

bool SameEvent(
    const SpatemMovementEvent& expected,
    const SpatemMovementEvent& actual)
{
    if (expected.phaseState != actual.phaseState ||
        expected.timing.has_value() != actual.timing.has_value())
    {
        return false;
    }

    if (!expected.timing)
    {
        return true;
    }

    return SameTiming(*expected.timing, *actual.timing);
}

bool SameIntersection(
    const SpatemIntersection& expected,
    const SpatemIntersection& actual)
{
    if (expected.region != actual.region ||
        expected.intersectionId != actual.intersectionId ||
        expected.revision != actual.revision ||
        expected.statusBits != actual.statusBits ||
        expected.minuteOfYear != actual.minuteOfYear ||
        expected.dSecond != actual.dSecond ||
        expected.movements.size() != actual.movements.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < expected.movements.size(); ++i)
    {
        const auto& expectedMovement = expected.movements[i];
        const auto& actualMovement = actual.movements[i];

        if (expectedMovement.signalGroup != actualMovement.signalGroup ||
            expectedMovement.events.size() != actualMovement.events.size())
        {
            return false;
        }

        for (std::size_t j = 0;
             j < expectedMovement.events.size();
             ++j)
        {
            if (!SameEvent(
                    expectedMovement.events[j],
                    actualMovement.events[j]))
            {
                return false;
            }
        }
    }

    return true;
}

bool CheckRoundTrip()
{
    const SpatemInput expected = MakeInput();

    const auto encoded = VanetzaSpatemCodec::Encode(expected);

    if (!encoded.success || encoded.bytes.empty())
    {
        std::cerr
            << "[FAIL] encode"
            << " success=" << encoded.success
            << " bytes=" << encoded.bytes.size()
            << " error=\"" << encoded.error << "\"\n";
        return false;
    }

    const auto decoded = VanetzaSpatemCodec::Decode(encoded.bytes);

    if (!decoded.success)
    {
        std::cerr
            << "[FAIL] decode"
            << " error=\"" << decoded.error << "\"\n";
        return false;
    }

    const auto& actual = decoded.message;

    if (actual.stationId != expected.stationId ||
        actual.intersections.size() != 2U)
    {
        std::cerr
            << "[FAIL] message_identity_or_intersection_count"
            << " expected_station=" << expected.stationId
            << " actual_station=" << actual.stationId
            << " expected_intersections=2"
            << " actual_intersections=" << actual.intersections.size()
            << '\n';
        return false;
    }

    for (std::size_t i = 0; i < expected.intersections.size(); ++i)
    {
        if (!SameIntersection(
                expected.intersections[i],
                actual.intersections[i]))
        {
            std::cerr
                << "[FAIL] intersection_semantics_preserved"
                << " index=" << i
                << " expected_id="
                << expected.intersections[i].intersectionId
                << " actual_id="
                << actual.intersections[i].intersectionId
                << '\n';
            return false;
        }
    }

    std::cout
        << "[PASS] multi_intersection_codec_roundtrip"
        << " station_id=" << actual.stationId
        << " intersection_count=" << actual.intersections.size()
        << " ids="
        << actual.intersections[0].intersectionId << ','
        << actual.intersections[1].intersectionId
        << " revisions="
        << static_cast<unsigned>(actual.intersections[0].revision) << ','
        << static_cast<unsigned>(actual.intersections[1].revision)
        << " signal_groups="
        << static_cast<unsigned>(
               actual.intersections[0].movements[0].signalGroup) << ','
        << static_cast<unsigned>(
               actual.intersections[1].movements[0].signalGroup)
        << '\n';

    return true;
}

} // namespace

int main()
{
    const bool passed = CheckRoundTrip();

    std::cout << "MULTI_INTERSECTION_CODEC_ROUNDTRIP_CASES=1\n";
    std::cout << "MULTI_INTERSECTION_CODEC_ROUNDTRIP_PASSED="
              << (passed ? 1 : 0) << '\n';
    std::cout << "MULTI_INTERSECTION_CODEC_ROUNDTRIP_FAILED="
              << (passed ? 0 : 1) << '\n';
    std::cout << "MULTI_INTERSECTION_CODEC_ROUNDTRIP="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
