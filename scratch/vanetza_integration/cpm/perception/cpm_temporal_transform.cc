#include "vanetza_integration/cpm/perception/cpm_temporal_transform.h"

#include <limits>

namespace
{

constexpr std::int64_t kMicrosecondsPerMillisecond = 1000;
constexpr std::int64_t kMaxTimestampItsMs = 4398046511103LL;
constexpr std::int64_t kMinMeasurementDeltaMs = -2048;
constexpr std::int64_t kMaxMeasurementDeltaMs = 2047;
constexpr std::int64_t kMaxObjectAgeMs = 1500;

bool IsValidTimestamp(std::int64_t value)
{
    return value >= 0 && value <= kMaxTimestampItsMs;
}

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}

} // namespace

bool
CpmTemporalTransform::BuildReferenceTime(
    std::int64_t itsStartClockUs,
    std::int64_t elapsedSimulationUs,
    std::int64_t& outputReferenceTimeItsMs,
    std::string& error)
{
    if (itsStartClockUs < 0)
    {
        return Reject(
            error,
            "ITS start clock must be non-negative");
    }

    if (elapsedSimulationUs < 0)
    {
        return Reject(
            error,
            "Elapsed simulation time must be non-negative");
    }

    if (itsStartClockUs >
        std::numeric_limits<std::int64_t>::max() -
            elapsedSimulationUs)
    {
        return Reject(
            error,
            "Absolute ITS time overflows int64 microseconds");
    }

    const std::int64_t absoluteTimeUs =
        itsStartClockUs + elapsedSimulationUs;

    // TimestampIts uses milliseconds. Sub-millisecond precision is
    // truncated explicitly rather than rounded implicitly.
    const std::int64_t referenceTimeMs =
        absoluteTimeUs / kMicrosecondsPerMillisecond;

    if (!IsValidTimestamp(referenceTimeMs))
    {
        return Reject(
            error,
            "Reference time is outside the TimestampIts range");
    }

    outputReferenceTimeItsMs = referenceTimeMs;
    error.clear();
    return true;
}

bool
CpmTemporalTransform::ComputeMeasurementDeltaTimeMs(
    std::int64_t referenceTimeItsMs,
    std::int64_t measurementTimeItsMs,
    std::int32_t& outputDeltaMs,
    std::string& error)
{
    if (!IsValidTimestamp(referenceTimeItsMs))
    {
        return Reject(
            error,
            "Reference time is outside the TimestampIts range");
    }

    if (!IsValidTimestamp(measurementTimeItsMs))
    {
        return Reject(
            error,
            "Measurement time is outside the TimestampIts range");
    }

    // Positive: measurement after the reference time.
    // Negative: measurement before the reference time.
    const std::int64_t deltaMs =
        measurementTimeItsMs - referenceTimeItsMs;

    if (deltaMs < kMinMeasurementDeltaMs ||
        deltaMs > kMaxMeasurementDeltaMs)
    {
        return Reject(
            error,
            "Measurement delta is outside the CPM R2 range");
    }

    outputDeltaMs = static_cast<std::int32_t>(deltaMs);
    error.clear();
    return true;
}

bool
CpmTemporalTransform::ComputeObjectAgeMs(
    std::int64_t referenceTimeItsMs,
    std::int64_t firstObservedTimeItsMs,
    std::int32_t& outputAgeMs,
    std::string& error)
{
    if (!IsValidTimestamp(referenceTimeItsMs))
    {
        return Reject(
            error,
            "Reference time is outside the TimestampIts range");
    }

    if (!IsValidTimestamp(firstObservedTimeItsMs))
    {
        return Reject(
            error,
            "First-observed time is outside the TimestampIts range");
    }

    if (firstObservedTimeItsMs > referenceTimeItsMs)
    {
        return Reject(
            error,
            "First-observed time is after the reference time");
    }

    // Object age measures how long this station has observed the object.
    // It is not the age of the latest sensor measurement.
    const std::int64_t ageMs =
        referenceTimeItsMs - firstObservedTimeItsMs;

    // CPM R2: 1500 ms is the saturation value for an older track.
    const std::int64_t saturatedAgeMs =
        ageMs > kMaxObjectAgeMs ? kMaxObjectAgeMs : ageMs;

    outputAgeMs = static_cast<std::int32_t>(saturatedAgeMs);
    error.clear();
    return true;
}
