#include "sumo_traffic_light_timing_mapper.h"

#include <vanetza/common/clock.hpp>
#include <chrono>

#include <cmath>
#include <cstdint>
#include <limits>

namespace
{

constexpr uint32_t kMaxMinuteOfYear = 527040U;
constexpr uint16_t kMaxValidDSecond = 59999U;

constexpr int64_t kMillisecondsPerSecond = 1000LL;
constexpr int64_t kMillisecondsPerMinute = 60000LL;
constexpr int64_t kMillisecondsPerHour = 3600000LL;
constexpr int64_t kMillisecondsPerTimeMark = 100LL;

bool
IsFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

} // namespace

namespace ns3
{

bool
SumoTrafficLightTimingMapper::BuildReference(
    const std::string& itsStartTime,
    int64_t elapsedMicroseconds,
    SumoSpatemTimeReference& output,
    std::string& error)
{
    if (itsStartTime.empty())
    {
        error = "SPATEM requires an explicit ITS start time";
        return false;
    }

    if (elapsedMicroseconds < 0)
    {
        error = "Simulation elapsed time must be non-negative";
        return false;
    }

    std::string normalized = itsStartTime;
    const auto separator = normalized.find('T');
    if (separator != std::string::npos)
    {
        normalized[separator] = ' ';
    }

    try
    {
        const auto start = vanetza::Clock::at(normalized);
        const auto startUs =
            start.time_since_epoch().count();

        if (startUs >
            std::numeric_limits<vanetza::Clock::rep>::max()
                - elapsedMicroseconds)
        {
            error = "Absolute ITS time overflows the Vanetza clock";
            return false;
        }

        const auto current =
            start + std::chrono::microseconds(elapsedMicroseconds);

        const auto timestamp = vanetza::Clock::at(current);

        if (timestamp.is_not_a_date_time())
        {
            error = "Absolute ITS time is not a valid date-time";
            return false;
        }

        const auto date = timestamp.date();
        const auto timeOfDay = timestamp.time_of_day();

        const uint64_t minuteOfYear =
            static_cast<uint64_t>(date.day_of_year() - 1) * 1440ULL
            + static_cast<uint64_t>(timeOfDay.hours()) * 60ULL
            + static_cast<uint64_t>(timeOfDay.minutes());

        const int64_t dSecond =
            timeOfDay.total_milliseconds() % 60000LL;

        if (minuteOfYear >= 527040ULL ||
            dSecond < 0 ||
            dSecond > 59999LL)
        {
            error = "Absolute ITS time is outside SPATEM reference range";
            return false;
        }

        output.minuteOfYear =
            static_cast<uint32_t>(minuteOfYear);
        output.dSecond =
            static_cast<uint16_t>(dSecond);

        error.clear();
        return true;
    }
    catch (...)
    {
        error = "Unable to parse ITS start time: " + itsStartTime;
        return false;
    }
}

bool
SumoTrafficLightTimingMapper::Map(
    const SumoTrafficLightState& input,
    const SumoSpatemTimeReference& reference,
    SpatemTiming& output,
    std::string& error)
{
    if (!IsFiniteNonNegative(input.simulationTime))
    {
        error = "SUMO simulationTime must be finite and non-negative";
        return false;
    }

    if (!IsFiniteNonNegative(input.nextSwitch))
    {
        error = "SUMO nextSwitch must be finite and non-negative";
        return false;
    }

    if (reference.minuteOfYear > kMaxMinuteOfYear)
    {
        error = "SPATEM minuteOfYear is out of range";
        return false;
    }

    if (reference.dSecond > kMaxValidDSecond)
    {
        error = "SPATEM dSecond is unavailable or out of range";
        return false;
    }

    if (input.nextSwitch + 1e-9 < input.simulationTime)
    {
        error = "SUMO nextSwitch is older than simulationTime";
        return false;
    }

    const double deltaSeconds =
        std::max(0.0, input.nextSwitch - input.simulationTime);

    const long double deltaMillisecondsExact =
        static_cast<long double>(deltaSeconds) *
        static_cast<long double>(kMillisecondsPerSecond);

    const long double maxInt64 =
        static_cast<long double>(
            std::numeric_limits<int64_t>::max());

    if (!std::isfinite(
            static_cast<double>(deltaMillisecondsExact)) ||
        deltaMillisecondsExact > maxInt64)
    {
        error = "SUMO timing delta cannot be represented safely";
        return false;
    }

    const int64_t deltaMilliseconds =
        static_cast<int64_t>(
            std::llround(deltaMillisecondsExact));

    const int64_t referenceMilliseconds =
        static_cast<int64_t>(reference.minuteOfYear) *
            kMillisecondsPerMinute +
        static_cast<int64_t>(reference.dSecond);

    if (deltaMilliseconds >
        std::numeric_limits<int64_t>::max() -
            referenceMilliseconds)
    {
        error = "absolute SPATEM timing overflows int64";
        return false;
    }

    const int64_t absoluteMilliseconds =
        referenceMilliseconds + deltaMilliseconds;

    const int64_t withinHourMilliseconds =
        absoluteMilliseconds % kMillisecondsPerHour;

    output = SpatemTiming{};
    output.minEndTime =
        static_cast<uint32_t>(
            withinHourMilliseconds / kMillisecondsPerTimeMark);

    // No uncertainty information is available from this SUMO contract.
    // Therefore maxEndTime, likelyTime and confidence remain unset.
    error.clear();
    return true;
}

} // namespace ns3
