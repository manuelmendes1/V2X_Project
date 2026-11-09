#include "vanetza_integration/sumo_adapter/sumo_person_datagram.h"

#include <charconv>
#include <cmath>
#include <string_view>
#include <system_error>
#include <vector>

namespace ns3
{
namespace
{

constexpr double kTraCIUnavailable = -1073741824.0;

std::vector<std::string_view> SplitCsv(const std::string& message)
{
    std::vector<std::string_view> fields;
    const std::string_view input(message);
    std::size_t start = 0;

    while (true)
    {
        const std::size_t delimiter = input.find(',', start);
        if (delimiter == std::string_view::npos)
        {
            fields.emplace_back(input.substr(start));
            break;
        }

        fields.emplace_back(input.substr(start, delimiter - start));
        start = delimiter + 1;
    }

    return fields;
}

bool ParseFiniteDouble(std::string_view token, double& output)
{
    if (token.empty())
    {
        return false;
    }

    double value{};
    const char* first = token.data();
    const char* last = first + token.size();
    const auto result = std::from_chars(
        first, last, value, std::chars_format::general);

    if (result.ec != std::errc{} ||
        result.ptr != last ||
        !std::isfinite(value))
    {
        return false;
    }

    output = value;
    return true;
}

bool ParseTimestampUs(std::string_view token, std::uint64_t& output)
{
    if (token.empty())
    {
        return false;
    }

    std::uint64_t value{};
    const char* first = token.data();
    const char* last = first + token.size();
    const auto result = std::from_chars(first, last, value, 10);

    if (result.ec != std::errc{} || result.ptr != last)
    {
        return false;
    }

    output = value;
    return true;
}

bool IsValidPersonId(std::string_view personId)
{
    return !personId.empty() &&
           personId.find('\r') == std::string_view::npos &&
           personId.find('\n') == std::string_view::npos;
}

} // namespace

std::optional<SumoPersonDatagram>
ParseSumoPersonDatagram(const std::string& message)
{
    const auto fields = SplitCsv(message);
    if (fields.size() < 4 ||
        fields[0] != "SUMO_PERSON" ||
        !IsValidPersonId(fields[2]))
    {
        return std::nullopt;
    }

    SumoPersonDatagram parsed;
    parsed.personId = std::string(fields[2]);

    if (fields[1] == "REMOVE")
    {
        if (fields.size() != 4 ||
            !ParseTimestampUs(fields[3], parsed.simulationTimeUs))
        {
            return std::nullopt;
        }

        parsed.eventType = SumoPersonEventType::Remove;
        parsed.sample.reset();
        return parsed;
    }

    if (fields[1] != "UPSERT" ||
        (fields.size() != 8 && fields.size() != 9))
    {
        return std::nullopt;
    }

    SumoPersonSample sample;
    if (!ParseFiniteDouble(fields[3], sample.x) ||
        !ParseFiniteDouble(fields[4], sample.y) ||
        !ParseFiniteDouble(fields[5], sample.speed) ||
        !ParseFiniteDouble(fields[6], sample.angleDeg) ||
        !ParseTimestampUs(fields[7], parsed.simulationTimeUs))
    {
        return std::nullopt;
    }

    // TraCI reports -2^30 when position, speed or angle is unavailable.
    // Treat the sentinel as missing data, not as a physical observation.
    if (sample.x == kTraCIUnavailable ||
        sample.y == kTraCIUnavailable ||
        sample.speed == kTraCIUnavailable ||
        sample.angleDeg == kTraCIUnavailable ||
        sample.speed < 0.0)
    {
        return std::nullopt;
    }

    // The ninth field is source metadata. Keep it opaque here: profile
    // interpretation belongs to SumoVruProfileMapper.
    if (fields.size() == 9)
    {
        const auto sumoVClass = fields[8];
        if (sumoVClass.empty() ||
            sumoVClass.find('\r') != std::string_view::npos ||
            sumoVClass.find('\n') != std::string_view::npos ||
            sumoVClass.find('\0') != std::string_view::npos)
        {
            return std::nullopt;
        }
        parsed.sumoVClass = std::string(sumoVClass);
    }

    parsed.eventType = SumoPersonEventType::Upsert;
    parsed.sample = sample;
    return parsed;
}

} // namespace ns3
