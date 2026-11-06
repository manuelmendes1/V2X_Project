#include "sumo_traffic_light_timing_mapper.h"

#include "vanetza_integration/context/its_time/its_start_time_parser.h"

#include <vanetza/common/clock.hpp>
#include <chrono>

#include <cmath>
#include <cstdint>
#include <limits>

namespace
{

constexpr uint32_t kMaxMinuteOfYear = 527040U;
constexpr uint16_t kMaxValidDSecond = 60999U;

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

    vanetza::Clock::time_point start;
    std::string parseError;

    if (!ItsStartTimeParser::ParseUtc(
            itsStartTime,
            start,
            parseError))
    {
        error = "Unable to parse UTC ITS start time: " + parseError;
        return false;
    }

    const std::int64_t startUs =
        start.time_since_epoch().count();

    const std::int64_t maxClock =
        std::numeric_limits<std::int64_t>::max();

    if (startUs < 0 ||
        elapsedMicroseconds > maxClock - startUs)
    {
        error = "Absolute ITS time overflows the Vanetza clock";
        return false;
    }

    const std::int64_t absoluteItsTimestampUs =
        startUs + elapsedMicroseconds;

    const vanetza::Clock::time_point current{
        std::chrono::microseconds(absoluteItsTimestampUs)
    };

    ItsUtcTimeReference utc;
    if (!ItsStartTimeParser::ConvertToUtc(
            current,
            utc,
            error))
    {
        error = "Unable to convert SPATEM reference to UTC: " + error;
        return false;
    }

    SumoSpatemTimeReference candidate;
    candidate.minuteOfYear = utc.minuteOfYear;
    candidate.dSecond = utc.dSecond;
    candidate.absoluteItsTimestampUs = absoluteItsTimestampUs;

    // Commit output only after parsing and conversion succeed.
    output = candidate;
    error.clear();
    return true;
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

    if (reference.minuteOfYear >= kMaxMinuteOfYear)
    {
        error = "SPATEM minuteOfYear is out of range";
        return false;
    }

    if (reference.dSecond > kMaxValidDSecond)
    {
        error = "SPATEM dSecond is unavailable or out of range";
        return false;
    }

    if (reference.absoluteItsTimestampUs < 0)
    {
        error = "SPATEM absolute ITS timestamp is missing";
        return false;
    }

    if (input.nextSwitch + 1e-9 < input.simulationTime)
    {
        error = "SUMO nextSwitch is older than simulationTime";
        return false;
    }

    const double deltaSeconds =
        std::max(0.0, input.nextSwitch - input.simulationTime);

    const long double deltaMicrosecondsExact =
        static_cast<long double>(deltaSeconds) * 1000000.0L;

    const long double maxInt64 =
        static_cast<long double>(
            std::numeric_limits<int64_t>::max());

    if (!std::isfinite(deltaMicrosecondsExact) ||
        deltaMicrosecondsExact > maxInt64)
    {
        error = "SUMO timing delta cannot be represented safely";
        return false;
    }

    const int64_t deltaMicroseconds =
        static_cast<int64_t>(
            std::llround(deltaMicrosecondsExact));

    // DSRC TimeMark 36000 represents a predicted change more than
    // one hour in the future. Do not wrap it into the current hour.
    constexpr int64_t maxTimeMarkHorizonUs =
        3600LL * 1000000LL;

    if (deltaMicroseconds >= maxTimeMarkHorizonUs)
    {
        SpatemTiming candidate{};
        candidate.minEndTime = 36000U;
        output = candidate;
        error.clear();
        return true;
    }

    const int64_t maxClock =
        std::numeric_limits<int64_t>::max();

    if (deltaMicroseconds >
        maxClock - reference.absoluteItsTimestampUs)
    {
        error = "absolute SPATEM event time overflows int64";
        return false;
    }

    const int64_t eventItsTimestampUs =
        reference.absoluteItsTimestampUs + deltaMicroseconds;

    const vanetza::Clock::time_point eventItsTime{
        std::chrono::microseconds(eventItsTimestampUs)
    };

    ItsUtcTimeReference eventUtc;
    if (!ItsStartTimeParser::ConvertToUtc(
            eventItsTime,
            eventUtc,
            error))
    {
        error = "Unable to convert SPATEM event time to UTC: " + error;
        return false;
    }

    SpatemTiming candidate{};
    candidate.minEndTime = eventUtc.timeMark;

    output = candidate;
    error.clear();
    return true;
}

} // namespace ns3
