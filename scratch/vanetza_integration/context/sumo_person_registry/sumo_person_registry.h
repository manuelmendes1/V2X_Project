#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "vanetza_integration/sumo_adapter/sumo_person_datagram.h"

namespace ns3
{

struct SumoPersonState
{
    SumoPersonSample sample;

    // Timestamp of the first accepted sample in the current active
    // observation period. Preserved across subsequent UPSERT events.
    std::uint64_t firstObservedAtUs{};

    // Timestamp of the latest accepted source event/sample.
    std::uint64_t simulationTimeUs{};
};

// Retains SUMO person observations without creating ns-3 or ITS identities.
class SumoPersonRegistry
{
public:
    bool Apply(const SumoPersonDatagram& datagram);

    std::optional<SumoPersonState> Find(const std::string& personId) const;

    std::size_t Size() const noexcept;

    bool Empty() const noexcept;

    void Clear() noexcept;

private:
    std::map<std::string, SumoPersonState> m_persons;

    // Keep the last accepted event time after REMOVE as a tombstone. This
    // prevents a delayed UDP UPSERT from resurrecting a departed person.
    std::map<std::string, std::uint64_t> m_lastEventTimeUs;
};

} // namespace ns3
