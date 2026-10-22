#include "sumo_vehicle_datagram.h"

#include <charconv>
#include <cmath>
#include <string_view>
#include <system_error>
#include <vector>

namespace ns3
{
namespace
{

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
        first,
        last,
        value,
        std::chars_format::general);

    if (result.ec != std::errc{} ||
        result.ptr != last ||
        !std::isfinite(value))
    {
        return false;
    }

    output = value;
    return true;
}

bool ParseTimestampUs(
    std::string_view token,
    std::uint64_t& output)
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

} // namespace

std::optional<SumoVehicleDatagram>
ParseSumoVehicleDatagram(const std::string& message)
{
    // Preserve empty fields, including a trailing comma, so malformed
    // optional timestamps and incomplete fields are rejected.
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

    if ((fields.size() != 6 &&
         fields.size() != 7 &&
         fields.size() != 9) ||
        fields[0].empty())
    {
        return std::nullopt;
    }

    SumoVehicleDatagram parsed;
    parsed.vehId = fields[0];

    if (!ParseFiniteDouble(fields[1], parsed.x) ||
        !ParseFiniteDouble(fields[2], parsed.y) ||
        !ParseFiniteDouble(fields[3], parsed.speed) ||
        !ParseFiniteDouble(fields[4], parsed.heading) ||
        !ParseFiniteDouble(fields[5], parsed.accel))
    {
        return std::nullopt;
    }

    if (fields.size() == 7 || fields.size() == 9)
    {
        std::uint64_t timestampUs{};

        if (!ParseTimestampUs(fields[6], timestampUs))
        {
            return std::nullopt;
        }

        parsed.simulationTimeUs = timestampUs;
    }

    if (fields.size() == 9)
    {
        double longitudeDeg{};
        double latitudeDeg{};

        if (!ParseFiniteDouble(fields[7], longitudeDeg) ||
            !ParseFiniteDouble(fields[8], latitudeDeg) ||
            longitudeDeg < -180.0 ||
            longitudeDeg > 180.0 ||
            latitudeDeg < -90.0 ||
            latitudeDeg > 90.0)
        {
            return std::nullopt;
        }

        parsed.longitudeDeg = longitudeDeg;
        parsed.latitudeDeg = latitudeDeg;
    }

    return parsed;
}

} // namespace ns3
