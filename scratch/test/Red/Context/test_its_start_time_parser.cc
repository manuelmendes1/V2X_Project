#include "vanetza_integration/context/its_time/its_start_time_parser.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{

bool
Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << label << '\n';
    return condition;
}

bool
CheckParse(
    const std::string& input,
    std::int64_t expectedUs,
    const char* label)
{
    vanetza::Clock::time_point output{};
    std::string error;

    const bool ok =
        ItsStartTimeParser::ParseUtc(input, output, error);

    return Check(
        ok &&
        output.time_since_epoch().count() == expectedUs &&
        error.empty(),
        label);
}

bool
CheckReject(
    const std::string& input,
    const char* label)
{
    constexpr std::int64_t sentinel = 123456789LL;

    vanetza::Clock::time_point output{
        std::chrono::microseconds(sentinel)
    };

    std::string error;

    const bool ok =
        ItsStartTimeParser::ParseUtc(input, output, error);

    return Check(
        !ok &&
        !error.empty() &&
        output.time_since_epoch().count() == sentinel,
        label);
}

} // namespace

int
main()
{
    bool allPassed = true;

    constexpr std::int64_t knownItsStartUs =
        719748005000000LL;

    allPassed &= CheckParse(
        "2004-01-01T00:00:00",
        0,
        "its_epoch_iso_t");

    allPassed &= CheckParse(
        "2004-01-01 00:00:00",
        0,
        "its_epoch_space");

    allPassed &= CheckParse(
        "2026-10-22T10:00:00",
        knownItsStartUs,
        "known_utc_iso_t");

    allPassed &= CheckParse(
        "2026-10-22 10:00:00",
        knownItsStartUs,
        "known_utc_space");

    // ETSI CDD TimestampIts example:
    // 2007-01-01T00:00:00 UTC = 94694401000 ms since ITS epoch.
    allPassed &= CheckParse(
        "2007-01-01T00:00:00",
        94694401000000LL,
        "known_timestamp_its_2007_after_first_leap_second");

    allPassed &= CheckParse(
        "2005-12-31T23:59:59",
        63158399000000LL,
        "before_first_leap_second");

    allPassed &= CheckParse(
        "2006-01-01T00:00:00",
        63158401000000LL,
        "after_first_leap_second");

    allPassed &= CheckParse(
        "2008-12-31T23:59:59",
        157852800000000LL,
        "before_second_leap_second");

    allPassed &= CheckParse(
        "2009-01-01T00:00:00",
        157852802000000LL,
        "after_second_leap_second");

    allPassed &= CheckParse(
        "2012-06-30T23:59:59",
        268185601000000LL,
        "before_third_leap_second");

    allPassed &= CheckParse(
        "2012-07-01T00:00:00",
        268185603000000LL,
        "after_third_leap_second");

    allPassed &= CheckParse(
        "2015-06-30T23:59:59",
        362793602000000LL,
        "before_fourth_leap_second");

    allPassed &= CheckParse(
        "2015-07-01T00:00:00",
        362793604000000LL,
        "after_fourth_leap_second");

    allPassed &= CheckParse(
        "2016-12-31T23:59:59",
        410313603000000LL,
        "before_fifth_leap_second");

    allPassed &= CheckParse(
        "2017-01-01T00:00:00",
        410313605000000LL,
        "after_fifth_leap_second");

    allPassed &= CheckReject("", "reject_empty");
    allPassed &= CheckReject(
        "2026-10-22T10:00:00Z",
        "reject_utc_suffix");
    allPassed &= CheckReject(
        "2026-10-22T10:00:00+01:00",
        "reject_positive_offset");
    allPassed &= CheckReject(
        "2026-10-22T10:00:00-05:00",
        "reject_negative_offset");
    allPassed &= CheckReject(
        "2026-10-22T10:00:00.123",
        "reject_fractional_seconds");
    allPassed &= CheckReject(
        "2026-02-30T10:00:00",
        "reject_invalid_calendar_date");
    allPassed &= CheckReject(
        "2026-10-22T25:00:00",
        "reject_invalid_hour");
    allPassed &= CheckReject(
        "2026-10-22T10:60:00",
        "reject_invalid_minute");
    allPassed &= CheckReject(
        "2026-10-22T10:00:60",
        "reject_invalid_second");
    allPassed &= CheckReject(
        "2026-10-22T10:00:00junk",
        "reject_trailing_data");
    allPassed &= CheckReject(
        "2003-12-31T23:59:59",
        "reject_before_its_epoch");

    std::cout
        << (allPassed
            ? "ITS_START_TIME_PARSER_CONTRACT=PASS\n"
            : "ITS_START_TIME_PARSER_CONTRACT=FAIL\n");

    return allPassed ? 0 : 1;
}
