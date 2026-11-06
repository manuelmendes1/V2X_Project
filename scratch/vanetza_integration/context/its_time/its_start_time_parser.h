#pragma once

#include <cstdint>
#include <string>

#include "vanetza/common/clock.hpp"

struct ItsUtcTimeReference
{
    std::uint32_t minuteOfYear = 0;
    std::uint16_t dSecond = 0;
    std::uint16_t timeMark = 0;
};

class ItsStartTimeParser
{
public:
    static bool ParseUtc(
        const std::string& input,
        vanetza::Clock::time_point& output,
        std::string& error);

    static bool ConvertToUtc(
        const vanetza::Clock::time_point& input,
        ItsUtcTimeReference& output,
        std::string& error);
};
