#include "vanetza_integration/cpm/perception/cpm_temporal_transform.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace
{

constexpr std::int64_t kMaxTimestampItsMs = 4398046511103LL;

bool CheckReferenceTime(
    const char* name,
    std::int64_t startUs,
    std::int64_t elapsedUs,
    std::int64_t expectedMs)
{
    std::int64_t output = -999;
    std::string error = "stale error";

    const bool accepted = CpmTemporalTransform::BuildReferenceTime(
        startUs, elapsedUs, output, error);

    const bool correct =
        accepted && output == expectedMs && error.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << name << '\n';

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " output=" << output
                  << " expected=" << expectedMs
                  << " error=" << error << '\n';
    }

    return correct;
}

bool CheckReferenceTimeRejected(
    const char* name,
    std::int64_t startUs,
    std::int64_t elapsedUs)
{
    constexpr std::int64_t sentinel = 777;
    std::int64_t output = sentinel;
    std::string error;

    const bool accepted = CpmTemporalTransform::BuildReferenceTime(
        startUs, elapsedUs, output, error);

    const bool correct =
        !accepted && !error.empty() && output == sentinel;

    std::cout << (correct ? "PASS " : "FAIL ")
              << name << '\n';

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " output=" << output
                  << " error=" << error << '\n';
    }

    return correct;
}

bool CheckMeasurementDelta(
    const char* name,
    std::int64_t referenceMs,
    std::int64_t measurementMs,
    std::int32_t expectedDeltaMs)
{
    std::int32_t output = 999;
    std::string error = "stale error";

    const bool accepted =
        CpmTemporalTransform::ComputeMeasurementDeltaTimeMs(
            referenceMs, measurementMs, output, error);

    const bool correct =
        accepted && output == expectedDeltaMs && error.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << name << '\n';

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " output=" << output
                  << " expected=" << expectedDeltaMs
                  << " error=" << error << '\n';
    }

    return correct;
}

bool CheckMeasurementDeltaRejected(
    const char* name,
    std::int64_t referenceMs,
    std::int64_t measurementMs)
{
    constexpr std::int32_t sentinel = 123;
    std::int32_t output = sentinel;
    std::string error;

    const bool accepted =
        CpmTemporalTransform::ComputeMeasurementDeltaTimeMs(
            referenceMs, measurementMs, output, error);

    const bool correct =
        !accepted && !error.empty() && output == sentinel;

    std::cout << (correct ? "PASS " : "FAIL ")
              << name << '\n';

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " output=" << output
                  << " error=" << error << '\n';
    }

    return correct;
}

bool CheckObjectAge(
    const char* name,
    std::int64_t referenceMs,
    std::int64_t firstObservedMs,
    std::int32_t expectedAgeMs)
{
    std::int32_t output = 999;
    std::string error = "stale error";

    const bool accepted = CpmTemporalTransform::ComputeObjectAgeMs(
        referenceMs, firstObservedMs, output, error);

    const bool correct =
        accepted && output == expectedAgeMs && error.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << name << '\n';

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " output=" << output
                  << " expected=" << expectedAgeMs
                  << " error=" << error << '\n';
    }

    return correct;
}

bool CheckObjectAgeRejected(
    const char* name,
    std::int64_t referenceMs,
    std::int64_t firstObservedMs)
{
    constexpr std::int32_t sentinel = 321;
    std::int32_t output = sentinel;
    std::string error;

    const bool accepted = CpmTemporalTransform::ComputeObjectAgeMs(
        referenceMs, firstObservedMs, output, error);

    const bool correct =
        !accepted && !error.empty() && output == sentinel;

    std::cout << (correct ? "PASS " : "FAIL ")
              << name << '\n';

    if (!correct)
    {
        std::cerr << "  accepted=" << accepted
                  << " output=" << output
                  << " error=" << error << '\n';
    }

    return correct;
}

} // namespace

int main()
{
    bool passed = true;

    // 1,000,001 us + 2,345,678 us = 3,345,679 us.
    // A TimestampIts em milissegundos trunca a fração restante.
    passed = CheckReferenceTime(
        "REFERENCE_TIME_KNOWN_INSTANT",
        1000001, 2345678, 3345) && passed;

    passed = CheckReferenceTime(
        "REFERENCE_TIME_EPOCH",
        0, 0, 0) && passed;

    passed = CheckReferenceTime(
        "REFERENCE_TIME_MAX_BOUNDARY",
        kMaxTimestampItsMs * 1000LL, 0,
        kMaxTimestampItsMs) && passed;

    passed = CheckReferenceTime(
        "REFERENCE_TIME_MAX_PLUS_999_US_TRUNCATES",
        kMaxTimestampItsMs * 1000LL + 999LL, 0,
        kMaxTimestampItsMs) && passed;

    passed = CheckReferenceTimeRejected(
        "REFERENCE_TIME_NEGATIVE_START", -1, 0) && passed;

    passed = CheckReferenceTimeRejected(
        "REFERENCE_TIME_NEGATIVE_ELAPSED", 1000000, -1) && passed;

    passed = CheckReferenceTimeRejected(
        "REFERENCE_TIME_OUT_OF_RANGE",
        (kMaxTimestampItsMs + 1) * 1000LL, 0) && passed;

    passed = CheckReferenceTimeRejected(
        "REFERENCE_TIME_MAX_PLUS_1000_US_REJECTED",
        kMaxTimestampItsMs * 1000LL + 1000LL, 0) && passed;

    passed = CheckReferenceTimeRejected(
        "REFERENCE_TIME_INTEGER_OVERFLOW",
        std::numeric_limits<std::int64_t>::max() - 5, 10) && passed;

    // Delta = measurement - reference: negative before, positive after.
    passed = CheckMeasurementDelta(
        "MEASUREMENT_DELTA_NEGATIVE",
        10000, 9990, -10) && passed;

    passed = CheckMeasurementDelta(
        "MEASUREMENT_DELTA_POSITIVE",
        10000, 10010, 10) && passed;

    passed = CheckMeasurementDelta(
        "MEASUREMENT_DELTA_MIN",
        10000, 7952, -2048) && passed;

    passed = CheckMeasurementDelta(
        "MEASUREMENT_DELTA_MAX",
        10000, 12047, 2047) && passed;

    passed = CheckMeasurementDeltaRejected(
        "MEASUREMENT_DELTA_BELOW_MIN",
        10000, 7951) && passed;

    passed = CheckMeasurementDeltaRejected(
        "MEASUREMENT_DELTA_ABOVE_MAX",
        10000, 12048) && passed;

    passed = CheckMeasurementDeltaRejected(
        "MEASUREMENT_DELTA_INVALID_REFERENCE",
        -1, 0) && passed;

    passed = CheckMeasurementDeltaRejected(
        "MEASUREMENT_DELTA_INVALID_MEASUREMENT",
        10000, kMaxTimestampItsMs + 1) && passed;

    passed = CheckMeasurementDeltaRejected(
        "MEASUREMENT_DELTA_FULL_RANGE_FORWARD_REJECTED",
        0, kMaxTimestampItsMs) && passed;

    passed = CheckMeasurementDeltaRejected(
        "MEASUREMENT_DELTA_FULL_RANGE_REVERSE_REJECTED",
        kMaxTimestampItsMs, 0) && passed;

    // A idade é medida desde a primeira observação, não desde a medição.
    passed = CheckObjectAge(
        "OBJECT_AGE_ZERO",
        10000, 10000, 0) && passed;

    passed = CheckObjectAge(
        "OBJECT_AGE_MAX",
        10000, 8500, 1500) && passed;

    passed = CheckObjectAge(
        "OBJECT_AGE_SATURATES_AFTER_MAX",
        10000, 7952, 1500) && passed;

    passed = CheckObjectAgeRejected(
        "OBJECT_AGE_FUTURE_FIRST_OBSERVATION",
        10000, 10001) && passed;

    passed = CheckObjectAge(
        "OBJECT_AGE_SATURATES_AT_LARGE_AGE",
        10000, 0, 1500) && passed;

    passed = CheckObjectAge(
        "OBJECT_AGE_FULL_TIMESTAMP_RANGE_SATURATES",
        kMaxTimestampItsMs, 0, 1500) && passed;

    passed = CheckObjectAgeRejected(
        "OBJECT_AGE_INVALID_REFERENCE",
        -1, 0) && passed;

    passed = CheckObjectAgeRejected(
        "OBJECT_AGE_INVALID_FIRST_OBSERVATION",
        10000, kMaxTimestampItsMs + 1) && passed;

    if (!passed)
    {
        return 1;
    }

    std::cout << "CPM_TEMPORAL_TRANSFORM=PASS\n";
    return 0;
}
