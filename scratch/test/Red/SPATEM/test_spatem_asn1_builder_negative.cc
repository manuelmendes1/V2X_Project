#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "vanetza_integration/wrappers/vanetza/spatem/spatem_asn1_builder.h"

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

bool Check(
    const std::string& name,
    const SpatemInput& input,
    bool expectedSuccess)
{
    const auto result = SpatemAsn1Builder::Build(input);
    const bool actualSuccess = static_cast<bool>(result);

    if (actualSuccess != expectedSuccess)
    {
        std::cerr << "[FAIL] " << name
                  << " expected_success=" << expectedSuccess
                  << " actual_success=" << actualSuccess << '\n';
        return false;
    }

    std::cout << "[PASS] " << name
              << " success=" << actualSuccess << '\n';
    return true;
}

bool CheckTwoIntersectionsPreserved()
{
    auto input = ValidInput();

    input.intersections[0].revision = 7;

    auto second = input.intersections[0];
    second.intersectionId = 43;
    second.revision = 8;
    input.intersections.push_back(second);

    const auto result = SpatemAsn1Builder::Build(input);
    if (!result)
    {
        std::cerr << "[FAIL] two_distinct_intersections_preserved"
                  << " reason=Build_returned_null\n";
        return false;
    }

    const auto& list = result->spat.intersections.list;
    bool ok = list.count == 2 && list.array != nullptr;

    if (ok)
    {
        ok = list.array[0] != nullptr && list.array[1] != nullptr;
    }

    if (ok)
    {
        ok =
            list.array[0]->id.id == 42 &&
            list.array[0]->revision == 7 &&
            list.array[1]->id.id == 43 &&
            list.array[1]->revision == 8;
    }

    if (ok)
    {
        std::cout << "[PASS] two_distinct_intersections_preserved\n";
    }
    else
    {
        std::cerr << "[FAIL] two_distinct_intersections_preserved"
                  << " expected_count=2 ids=42,43 revisions=7,8\n";
    }

    return ok;
}

SpatemInput WithTiming(const SpatemTiming& timing)
{
    auto input = ValidInput();
    input.intersections[0].movements[0].events[0].timing = timing;
    return input;
}
} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    const auto run = [&passed, &failed](
        const std::string& name,
        const SpatemInput& input,
        bool expectedSuccess)
    {
        if (Check(name, input, expectedSuccess))
        {
            ++passed;
        }
        else
        {
            ++failed;
        }
    };

    // Control case: reject-invalid tests must not reject valid input.
    run("valid_minimal_input", ValidInput(), true);

    {
        auto input = ValidInput();
        input.intersections[0].revision = 128;
        run("revision_above_msg_count_maximum", input, false);
        input.intersections[0].revision = 127;
        run("revision_maximum_valid_value", input, true);
    }

    {
        if (CheckTwoIntersectionsPreserved())
        {
            ++passed;
        }
        else
        {
            ++failed;
        }
    }

    {
        auto input = ValidInput();
        input.intersections.clear();
        run("empty_intersection_list", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].intersectionId = 0;
        run("intersection_id_zero", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].movements.clear();
        run("empty_movement_list", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].statusBits = 0x4000U;
        run("reserved_intersection_status_bit", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].minuteOfYear = 527040U;
        run("minute_of_year_invalid_sentinel", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].dSecond = 61000U;
        run("dsecond_reserved_lower_boundary", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].dSecond = 65534U;
        run("dsecond_reserved_upper_boundary", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].movements[0].events.clear();
        run("empty_movement_event_list", input, false);
    }
    {
        auto input = ValidInput();
        input.intersections[0].movements[0].events[0].phaseState =
            static_cast<SpatemMovementPhaseState>(10);
        run("unsupported_phase_state", input, false);
    }
    {
        SpatemTiming timing;
        timing.minEndTime = 36002U;
        run("min_end_time_out_of_range", WithTiming(timing), false);
    }
    {
        SpatemTiming timing;
        timing.minEndTime = 100U;
        timing.maxEndTime = 99U;
        run("max_end_time_before_minimum", WithTiming(timing), false);
    }
    {
        SpatemTiming timing;
        timing.minEndTime = 100U;
        timing.maxEndTime = 36002U;
        run("max_end_time_out_of_range", WithTiming(timing), false);
    }
    {
        SpatemTiming timing;
        timing.minEndTime = 100U;
        timing.maxEndTime = 200U;
        timing.likelyTime = 201U;
        run("likely_time_above_maximum", WithTiming(timing), false);
    }
    {
        SpatemTiming timing;
        timing.minEndTime = 100U;
        timing.likelyTime = 99U;
        run("likely_time_before_minimum", WithTiming(timing), false);
    }
    {
        SpatemTiming timing;
        timing.confidence = 16U;
        run("confidence_out_of_range", WithTiming(timing), false);
    }
    {
        SpatemTiming timing;
        timing.startTime = 36002U;
        run("start_time_out_of_range", WithTiming(timing), false);
    }
    {
        SpatemTiming timing;
        timing.nextTime = 36002U;
        run("next_time_out_of_range", WithTiming(timing), false);
    }
    {
        auto input = ValidInput();
        for (unsigned i = 0; i < 32; ++i)
        {
            input.intersections.push_back(input.intersections.front());
        }
        // Initial valid intersection plus 32 additions = 33.
        run("more_than_32_intersections", input, false);
    }
    {
        auto input = ValidInput();
        auto& movements = input.intersections[0].movements;
        for (unsigned i = 0; i < 255; ++i)
        {
            movements.push_back(movements.front());
        }
        // Initial movement plus 255 additions = 256.
        run("more_than_255_movements", input, false);
    }
    {
        SpatemTiming timing;
        timing.minEndTime = 36001U;
        timing.maxEndTime = 36001U;
        timing.likelyTime = 36001U;
        timing.startTime = 36001U;
        timing.nextTime = 36001U;
        timing.confidence = 15U;
        run("valid_timing_upper_boundaries", WithTiming(timing), true);
    }
    {
        auto input = ValidInput();
        input.intersections[0].minuteOfYear = 527039U;
        run("minute_of_year_maximum_valid_value", input, true);
    }
    {
        auto input = ValidInput();
        input.intersections[0].dSecond = 60999U;
        run("dsecond_maximum_normal_value", input, true);
    }
    {
        auto input = ValidInput();
        input.intersections[0].dSecond = 65535U;
        run("dsecond_unavailable_sentinel", input, true);
    }

    std::cout << "SPATEM_BUILDER_CASES=" << (passed + failed) << '\n'
              << "SPATEM_BUILDER_PASSED=" << passed << '\n'
              << "SPATEM_BUILDER_FAILED=" << failed << '\n';

    if (failed == 0)
    {
        std::cout << "SPATEM_BUILDER_NEGATIVE_TESTS=PASS\n";
        return 0;
    }

    std::cout << "SPATEM_BUILDER_NEGATIVE_TESTS=FAIL\n";
    return 1;
}
