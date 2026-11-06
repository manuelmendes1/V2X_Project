#include "vanetza_integration/context/its_time/its_start_time_parser.h"

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <limits>
#include <string>

namespace
{

constexpr std::int64_t kMicrosecondsPerSecond = 1000000LL;

constexpr const char* kLeapEffectiveUtcDates[] = {
    "2006-01-01 00:00:00",
    "2009-01-01 00:00:00",
    "2012-07-01 00:00:00",
    "2015-07-01 00:00:00",
    "2017-01-01 00:00:00"
};

bool
HasStrictDateTimeFormat(const std::string& value)
{
    if (value.size() != 19)
    {
        return false;
    }

    constexpr std::size_t separators[] = {
        4, 7, 10, 13, 16
    };

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        bool isSeparator = false;

        for (const auto separator : separators)
        {
            if (i == separator)
            {
                isSeparator = true;
                break;
            }
        }

        if (isSeparator)
        {
            const char expected =
                (i == 10) ? ' ' : (i == 13 || i == 16) ? ':' : '-';

            if (value[i] != expected &&
                !(i == 10 && value[i] == 'T'))
            {
                return false;
            }
        }
        else if (value[i] < '0' || value[i] > '9')
        {
            return false;
        }
    }

    const auto parseTwoDigits =
        [&value](std::size_t position) -> int
        {
            return (value[position] - '0') * 10
                + (value[position + 1] - '0');
        };

    const int hour = parseTwoDigits(11);
    const int minute = parseTwoDigits(14);
    const int second = parseTwoDigits(17);

    if (hour > 23 || minute > 59 || second > 59)
    {
        return false;
    }

    return true;
}

} // namespace

bool
ItsStartTimeParser::ParseUtc(
    const std::string& input,
    vanetza::Clock::time_point& output,
    std::string& error)
{
    if (!HasStrictDateTimeFormat(input))
    {
        error = "ITS start time must use YYYY-MM-DDTHH:MM:SS "
                "or YYYY-MM-DD HH:MM:SS";
        return false;
    }

    std::string normalized = input;
    if (normalized[10] == 'T')
    {
        normalized[10] = ' ';
    }

    try
    {
        const auto parsed = vanetza::Clock::at(normalized);
        const std::int64_t parsedUs =
            parsed.time_since_epoch().count();

        if (parsedUs < 0)
        {
            error = "ITS start time cannot precede the ITS epoch";
            return false;
        }

        // The initial TAI-UTC offset at the ITS epoch is the baseline.
        // Only positive leap seconds introduced after the ITS epoch
        // are added here.
        std::int64_t elapsedLeapSeconds = 0;

        for (const char* effectiveUtc : kLeapEffectiveUtcDates)
        {
            if (normalized >= effectiveUtc)
            {
                ++elapsedLeapSeconds;
            }
        }

        const auto maxClock =
            std::numeric_limits<std::int64_t>::max();

        if (elapsedLeapSeconds >
            (maxClock - parsedUs) / kMicrosecondsPerSecond)
        {
            error = "ITS start time overflows after leap-second correction";
            return false;
        }

        const std::int64_t correctedUs =
            parsedUs + elapsedLeapSeconds * kMicrosecondsPerSecond;

        // Commit output only after parsing and correction succeed.
        output = vanetza::Clock::time_point{
            std::chrono::microseconds(correctedUs)
        };

        error.clear();
        return true;
    }
    catch (const std::exception&)
    {
        error = "Invalid ITS start time: " + input;
        return false;
    }
    catch (...)
    {
        error = "Unable to parse ITS start time: " + input;
        return false;
    }
}

bool
ItsStartTimeParser::ConvertToUtc(
    const vanetza::Clock::time_point& input,
    ItsUtcTimeReference& output,
    std::string& error)
{
    const std::int64_t itsUs =
        input.time_since_epoch().count();

    if (itsUs < 0)
    {
        error = "ITS timestamp cannot precede the ITS epoch";
        return false;
    }

    try
    {
        std::int64_t completedLeapSeconds = 0;
        const auto maxClock =
            std::numeric_limits<std::int64_t>::max();

        for (std::size_t index = 0;
             index < sizeof(kLeapEffectiveUtcDates) /
                         sizeof(kLeapEffectiveUtcDates[0]);
             ++index)
        {
            const auto boundary =
                vanetza::Clock::at(
                    std::string(kLeapEffectiveUtcDates[index]));

            const std::int64_t boundaryUs =
                boundary.time_since_epoch().count();

            const std::int64_t priorLeapOffset =
                static_cast<std::int64_t>(index) *
                kMicrosecondsPerSecond;

            if (boundaryUs >
                maxClock - priorLeapOffset -
                    kMicrosecondsPerSecond)
            {
                error = "Leap-second boundary overflows ITS clock";
                return false;
            }

            // The leap interval occupies one continuous ITS second:
            // [boundary + previous_leaps, boundary + current_leaps).
            const std::int64_t leapStartUs =
                boundaryUs + priorLeapOffset;

            const std::int64_t leapEndUs =
                leapStartUs + kMicrosecondsPerSecond;

            if (itsUs >= leapStartUs && itsUs < leapEndUs)
            {
                // The civil date/minute remains the final UTC minute
                // before the effective date. DSecond identifies the
                // inserted second explicitly as 60000..60999.
                const auto previousSecond =
                    vanetza::Clock::at(
                        vanetza::Clock::time_point{
                            std::chrono::microseconds(
                                boundaryUs - kMicrosecondsPerSecond)
                        });

                if (previousSecond.is_not_a_date_time())
                {
                    error = "Unable to represent UTC leap-second date";
                    return false;
                }

                const auto date = previousSecond.date();
                const auto timeOfDay = previousSecond.time_of_day();

                const std::int64_t minuteOfYear =
                    static_cast<std::int64_t>(
                        date.day_of_year() - 1) * 1440LL
                    + static_cast<std::int64_t>(
                        timeOfDay.hours()) * 60LL
                    + static_cast<std::int64_t>(
                        timeOfDay.minutes());

                const std::int64_t leapMilliseconds =
                    (itsUs - leapStartUs) / 1000LL;

                ItsUtcTimeReference candidate;
                candidate.minuteOfYear =
                    static_cast<std::uint32_t>(minuteOfYear);

                candidate.dSecond =
                    static_cast<std::uint16_t>(
                        60000LL + leapMilliseconds);

                // DSRC reserves 35991..35999 for the leap-second
                // condition. This implementation uses 35991 as the
                // canonical category marker; the individual reserved
                // values are not a defined subsecond progression.
                // DSecond retains the millisecond position when this
                // timestamp itself is inside the inserted second.
                candidate.timeMark = 35991U;

                output = candidate;
                error.clear();
                return true;
            }

            if (itsUs >= leapEndUs)
            {
                ++completedLeapSeconds;
            }
            else
            {
                break;
            }
        }

        // Outside an inserted second, remove all leap seconds that
        // have fully elapsed to recover the ordinary UTC civil clock.
        const std::int64_t utcUs =
            itsUs -
            completedLeapSeconds * kMicrosecondsPerSecond;

        const auto timestamp =
            vanetza::Clock::at(
                vanetza::Clock::time_point{
                    std::chrono::microseconds(utcUs)
                });

        if (timestamp.is_not_a_date_time())
        {
            error = "ITS timestamp is not a valid UTC date-time";
            return false;
        }

        const auto date = timestamp.date();
        const auto timeOfDay = timestamp.time_of_day();

        const std::int64_t minuteOfYear =
            static_cast<std::int64_t>(
                date.day_of_year() - 1) * 1440LL
            + static_cast<std::int64_t>(
                timeOfDay.hours()) * 60LL
            + static_cast<std::int64_t>(
                timeOfDay.minutes());

        const std::int64_t totalMilliseconds =
            timeOfDay.total_milliseconds();

        const std::int64_t dSecond =
            totalMilliseconds % 60000LL;

        const std::int64_t timeMark =
            (totalMilliseconds % 3600000LL) / 100LL;

        if (minuteOfYear < 0 ||
            minuteOfYear >= 527040LL ||
            dSecond < 0 ||
            dSecond > 59999LL ||
            timeMark < 0 ||
            timeMark > 35999LL)
        {
            error = "Converted UTC timestamp is outside SPATEM range";
            return false;
        }

        ItsUtcTimeReference candidate;
        candidate.minuteOfYear =
            static_cast<std::uint32_t>(minuteOfYear);
        candidate.dSecond =
            static_cast<std::uint16_t>(dSecond);
        candidate.timeMark =
            static_cast<std::uint16_t>(timeMark);

        output = candidate;
        error.clear();
        return true;
    }
    catch (const std::exception&)
    {
        error = "Unable to convert ITS timestamp to UTC";
        return false;
    }
    catch (...)
    {
        error = "Unexpected failure converting ITS timestamp to UTC";
        return false;
    }
}
