#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace ns3
{

// Internal application-level identity of a logical DENM event.
//
// This identity is deliberately independent from:
//   - DENM ActionID
//   - stationID
//   - GN_ADDR
//   - certificate identity
//   - ns-3 packet identity
//
// It identifies the originating vehicle/event pair inside the
// application/lifecycle domain.
struct DenmEventId
{
    std::string vehicleId;
    std::string eventId;

    bool operator==(const DenmEventId& other) const noexcept
    {
        return vehicleId == other.vehicleId &&
               eventId == other.eventId;
    }

    bool operator!=(const DenmEventId& other) const noexcept
    {
        return !(*this == other);
    }
};

struct DenmEventIdHash
{
    std::size_t operator()(
        const DenmEventId& id) const noexcept
    {
        const std::size_t vehicleHash =
            std::hash<std::string>{}(id.vehicleId);

        const std::size_t eventHash =
            std::hash<std::string>{}(id.eventId);

        return vehicleHash ^
               (eventHash + 0x9e3779b9u +
                (vehicleHash << 6) +
                (vehicleHash >> 2));
    }
};

} // namespace ns3
