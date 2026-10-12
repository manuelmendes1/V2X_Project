#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace ns3
{

struct SpatemReplayKey
{
    uint32_t stationId = 0;
    std::optional<uint16_t> region;
    uint16_t intersectionId = 0;

    bool operator==(const SpatemReplayKey& other) const noexcept
    {
        return stationId == other.stationId &&
               region == other.region &&
               intersectionId == other.intersectionId;
    }
};

struct SpatemReplayKeyHash
{
    std::size_t operator()(
        const SpatemReplayKey& key) const noexcept
    {
        std::size_t hash =
            static_cast<std::size_t>(key.stationId);

        hash ^= static_cast<std::size_t>(
                    key.intersectionId) +
                0x9e3779b9U +
                (hash << 6U) +
                (hash >> 2U);

        const std::size_t regionHash =
            key.region.has_value()
                ? static_cast<std::size_t>(*key.region) + 1U
                : 0U;

        hash ^= regionHash +
                0x9e3779b9U +
                (hash << 6U) +
                (hash >> 2U);

        return hash;
    }
};

struct SpatemReplayEntry
{
    bool hasTimestamp = false;
    bool semanticTimestamp = false;

    uint64_t timestamp = 0;

    std::unordered_set<uint64_t> fingerprints;
};

struct SpatemReplayContext
{
    std::unordered_map<
        SpatemReplayKey,
        SpatemReplayEntry,
        SpatemReplayKeyHash>
        database;
};

} // namespace ns3
