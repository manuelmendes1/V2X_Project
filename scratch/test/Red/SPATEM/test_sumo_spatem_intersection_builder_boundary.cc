#include <cstdint>
#include <iostream>
#include <string>

#include "sumo_adapter/sumo_traffic_light_spatem_intersection_builder.h"
#include "sumo_adapter/sumo_traffic_light_timing_mapper.h"

namespace
{

ns3::SumoTrafficLightState MakeState(uint32_t linkIndex)
{
    ns3::SumoTrafficLightState state;
    state.trafficLightId = "tls0";
    state.linkIndex = linkIndex;
    state.rawState = 'G';
    state.simulationTime = 1.0;
    state.nextSwitch = 3.0;
    return state;
}

bool CheckValidLinkAccepted()
{
    ns3::SumoSpatemIntersectionConfig config;
    config.trafficLightId = "tls0";
    config.intersectionId = 42;
    config.revision = 7;

    ns3::SumoTrafficLightSpatemIntersectionBuilder builder(config);

    ns3::SumoSpatemTimeReference reference;
    reference.minuteOfYear = 100;
    reference.dSecond = 1000;
    // Synthetic absolute reference at the ITS epoch for this boundary test.
    reference.absoluteItsTimestampUs = 0;

    std::string error;
    const bool accepted =
        builder.Update(MakeState(7), reference, error);

    const auto& intersection = builder.GetIntersection();

    const bool correct =
        accepted &&
        error.empty() &&
        intersection.intersectionId == 42 &&
        intersection.revision == 7 &&
        intersection.movements.size() == 1 &&
        intersection.movements[0].signalGroup == 7 &&
        intersection.movements[0].events.size() == 1;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << "valid_link_index_accepted\n";
    return correct;
}

bool CheckInvalidLinkRejectedWithoutChangingState()
{
    ns3::SumoSpatemIntersectionConfig config;
    config.trafficLightId = "tls0";
    config.intersectionId = 42;
    config.revision = 7;

    ns3::SumoTrafficLightSpatemIntersectionBuilder builder(config);

    ns3::SumoSpatemTimeReference reference;
    reference.minuteOfYear = 100;
    reference.dSecond = 1000;
    // Synthetic absolute reference at the ITS epoch for this boundary test.
    reference.absoluteItsTimestampUs = 0;

    std::string error;

    if (!builder.Update(MakeState(7), reference, error))
    {
        std::cerr
            << "[FAIL] link_256_rejected_and_state_preserved"
            << " reason=valid_baseline_update_failed error="
            << error << '\n';
        return false;
    }

    auto invalid = MakeState(256);
    invalid.rawState = 'R';

    error.clear();
    const bool accepted = builder.Update(invalid, reference, error);

    const auto& intersection = builder.GetIntersection();

    const bool correct =
        !accepted &&
        !error.empty() &&
        intersection.intersectionId == 42 &&
        intersection.revision == 7 &&
        intersection.movements.size() == 1 &&
        intersection.movements[0].signalGroup == 7 &&
        intersection.movements[0].events.size() == 1 &&
        intersection.movements[0].events[0].phaseState ==
            SpatemMovementPhaseState::PROTECTED_MOVEMENT_ALLOWED;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << "link_256_rejected_and_state_preserved\n";

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " error=" << error
                  << " movement_count=" << intersection.movements.size()
                  << '\n';
    }

    return correct;
}


bool CheckUtcSpatemTimeReference(
    const std::string& utc,
    uint32_t expectedMinuteOfYear,
    uint16_t expectedDSecond,
    const char* label,
    std::int64_t elapsedMicroseconds = 0)
{
    ns3::SumoSpatemTimeReference reference{};
    std::string error;

    const bool accepted =
        ns3::SumoTrafficLightTimingMapper::BuildReference(
            utc,
            elapsedMicroseconds,
            reference,
            error);

    const bool correct =
        accepted &&
        error.empty() &&
        reference.minuteOfYear == expectedMinuteOfYear &&
        reference.dSecond == expectedDSecond;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << label << '\n';

    if (!correct)
    {
        std::cerr
            << "  accepted=" << accepted
            << " minuteOfYear=" << reference.minuteOfYear
            << " expectedMinuteOfYear=" << expectedMinuteOfYear
            << " dSecond=" << reference.dSecond
            << " expectedDSecond=" << expectedDSecond
            << " error=" << error << '\n';
    }

    return correct;
}


bool CheckUtcSpatemMinEndTime(
    const std::string& utcStart,
    std::int64_t elapsedReferenceUs,
    double simulationTime,
    double nextSwitch,
    uint32_t expectedMinEndTime,
    uint32_t expectedMaxEndTime,
    const char* label)
{
    ns3::SumoSpatemTimeReference reference{};
    std::string error;

    const bool referenceBuilt =
        ns3::SumoTrafficLightTimingMapper::BuildReference(
            utcStart,
            elapsedReferenceUs,
            reference,
            error);

    if (!referenceBuilt)
    {
        std::cerr << "[FAIL] " << label
                  << " reference_error=" << error << '\n';
        return false;
    }

    auto state = MakeState(7);
    state.simulationTime = simulationTime;
    state.nextSwitch = nextSwitch;

    SpatemTiming timing{};
    const bool mapped =
        ns3::SumoTrafficLightTimingMapper::Map(
            state,
            reference,
            timing,
            error);

    const bool correct =
        mapped &&
        error.empty() &&
        timing.minEndTime >= expectedMinEndTime &&
        timing.minEndTime <= expectedMaxEndTime;

    std::cout << (correct ? "[PASS] " : "[FAIL] ")
              << label << '\n';

    if (!correct)
    {
        std::cerr
            << "  mapped=" << mapped
            << " minEndTime=" << timing.minEndTime
            << " expected_range=[" << expectedMinEndTime
            << "," << expectedMaxEndTime << "]"
            << " reference_minute=" << reference.minuteOfYear
            << " reference_dSecond=" << reference.dSecond
            << " error=" << error << '\n';
    }

    return correct;
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    if (CheckValidLinkAccepted())
        ++passed;
    else
        ++failed;

    if (CheckInvalidLinkRejectedWithoutChangingState())
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2005-12-31T23:59:59",
            525599U,
            59000U,
            "UTC reference before first post-epoch leap second"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2006-01-01T00:00:00",
            0U,
            0U,
            "UTC reference at first leap-second boundary"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2005-12-31T23:59:59",
            525599U,
            60000U,
            "first leap second start",
            1000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2005-12-31T23:59:59",
            525599U,
            60999U,
            "first leap second final millisecond",
            1999000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2005-12-31T23:59:59",
            0U,
            0U,
            "UTC immediately after first leap second",
            2000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2016-12-31T23:59:59",
            527039U,
            59000U,
            "UTC reference immediately before 2017 leap second"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2017-01-01T00:00:00",
            0U,
            0U,
            "UTC reference at 2017 year boundary"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2016-12-31T23:59:59",
            527039U,
            60000U,
            "2017 leap second start",
            1000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2016-12-31T23:59:59",
            527039U,
            60999U,
            "2017 leap second final millisecond",
            1999000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2016-12-31T23:59:59",
            0U,
            0U,
            "UTC immediately after 2017 leap second",
            2000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemMinEndTime(
            "2016-12-31T23:59:59",
            0,
            0.0,
            1.0,
            35991U,
            35991U,
            "TimeMark at start of 2017 leap second"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemMinEndTime(
            "2016-12-31T23:59:59",
            0,
            0.0,
            1.5,
            35991U,
            35991U,
            "TimeMark halfway through 2017 leap second"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemMinEndTime(
            "2016-12-31T23:59:59",
            0,
            0.0,
            1.999,
            35991U,
            35991U,
            "TimeMark at end of 2017 leap second"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemMinEndTime(
            "2016-12-31T23:59:59",
            0,
            0.0,
            2.0,
            0U,
            0U,
            "TimeMark immediately after 2017 leap second"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemMinEndTime(
            "2017-01-01T00:00:00",
            0,
            0.0,
            1.5,
            15U,
            15U,
            "TimeMark normal UTC time after 2017 leap second"))
        ++passed;
    else
        ++failed;

    // Remaining historical positive leap seconds: 2009, 2012 and 2015.
    if (CheckUtcSpatemTimeReference(
            "2008-12-31T23:59:59",
            527039U,
            60000U,
            "2009 leap second start",
            1000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2008-12-31T23:59:59",
            0U,
            0U,
            "UTC immediately after 2009 leap second",
            2000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2012-06-30T23:59:59",
            262079U,
            60000U,
            "2012 leap second start",
            1000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2012-06-30T23:59:59",
            262080U,
            0U,
            "UTC immediately after 2012 leap second",
            2000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2015-06-30T23:59:59",
            260639U,
            60000U,
            "2015 leap second start",
            1000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemTimeReference(
            "2015-06-30T23:59:59",
            260640U,
            0U,
            "UTC immediately after 2015 leap second",
            2000000LL))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemMinEndTime(
            "2017-01-01T00:00:00",
            0,
            0.0,
            3600.0,
            36000U,
            36000U,
            "TimeMark at exactly one hour"))
        ++passed;
    else
        ++failed;

    if (CheckUtcSpatemMinEndTime(
            "2017-01-01T00:00:00",
            0,
            0.0,
            3600.1,
            36000U,
            36000U,
            "TimeMark for event more than one hour ahead"))
        ++passed;
    else
        ++failed;

    std::cout << "SPATEM_SUMO_BOUNDARY_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_SUMO_BOUNDARY_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_SUMO_BOUNDARY_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_SUMO_BOUNDARY_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
