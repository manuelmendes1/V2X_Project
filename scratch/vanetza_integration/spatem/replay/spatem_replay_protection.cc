#include "spatem_replay_protection.h"

#include <string>

#include "vanetza_integration/core/v2x_log.h"

namespace
{

uint64_t
Fnv1a64(const std::vector<uint8_t>& bytes)
{
    constexpr uint64_t offset =
        14695981039346656037ULL;

    constexpr uint64_t prime =
        1099511628211ULL;

    uint64_t hash = offset;

    for (uint8_t byte : bytes)
    {
        hash ^= static_cast<uint64_t>(byte);
        hash *= prime;
    }

    return hash;
}

} // namespace

namespace ns3
{

bool
SpatemReplayProtection::Process(
    PacketContext& context,
    const SpatemMessage& message,
    SpatemReplayContext& replay)
{
    if (message.stationId == 0 ||
        message.intersections.empty())
    {
        context.MarkFailure();
        context.reason =
            "invalid_spatem_replay_identity";

        V2XWriteLog(
            context,
            V2XLogLevel::WARN,
            "SPATEM",
            "SPATEM_REPLAY_FAIL",
            "invalid station/intersection identity");

        return false;
    }

    const uint64_t fingerprint =
        Fingerprint(context);

    /*
     * Validate the entire message before changing replay state.
     * This prevents partial state updates when a multi-intersection
     * SPATEM contains one replayed intersection.
     */
    for (const auto& intersection :
         message.intersections)
    {
        SpatemReplayKey key{
            message.stationId,
            intersection.region,
            intersection.intersectionId};

        auto it =
            replay.database.find(key);

        if (it == replay.database.end())
        {
            continue;
        }

        const SpatemReplayEntry& entry =
            it->second;

        const bool semanticTimestamp =
            HasSemanticTimestamp(intersection);

        const uint64_t timestamp =
            semanticTimestamp
                ? SemanticTimestamp(intersection)
                : context.timestamp;

        if (!entry.hasTimestamp)
        {
            continue;
        }

        if (timestamp < entry.timestamp)
        {
            context.MarkDrop();
            context.reason =
                "spatem_timestamp_older_than_last";

            V2XWriteLog(
                context,
                V2XLogLevel::WARN,
                "SPATEM",
                "SPATEM_REPLAY_DROP",
                "older timestamp station=" +
                    std::to_string(message.stationId) +
                " intersection=" +
                    std::to_string(
                        intersection.intersectionId));

            return false;
        }

        if (timestamp == entry.timestamp &&
            entry.fingerprints.find(fingerprint) !=
                entry.fingerprints.end())
        {
            context.MarkDrop();
            context.reason =
                "spatem_replay_detected";

            V2XWriteLog(
                context,
                V2XLogLevel::WARN,
                "SPATEM",
                "SPATEM_REPLAY_DROP",
                "duplicate fingerprint station=" +
                    std::to_string(message.stationId) +
                " intersection=" +
                    std::to_string(
                        intersection.intersectionId));

            return false;
        }
    }

    /*
     * Apply only after all intersections have passed validation.
     */
    for (const auto& intersection :
         message.intersections)
    {
        SpatemReplayKey key{
            message.stationId,
            intersection.region,
            intersection.intersectionId};

        SpatemReplayEntry& entry =
            replay.database[key];

        const bool semanticTimestamp =
            HasSemanticTimestamp(intersection);

        const uint64_t timestamp =
            semanticTimestamp
                ? SemanticTimestamp(intersection)
                : context.timestamp;

        if (!entry.hasTimestamp ||
            timestamp > entry.timestamp)
        {
            entry.hasTimestamp = true;
            entry.semanticTimestamp =
                semanticTimestamp;
            entry.timestamp = timestamp;
            entry.fingerprints.clear();
        }

        entry.fingerprints.insert(fingerprint);
    }

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::INFO,
        "SPATEM",
        "SPATEM_REPLAY_OK",
        "station=" +
            std::to_string(message.stationId) +
        " intersections=" +
            std::to_string(message.intersections.size()));

    return true;
}

uint64_t
SpatemReplayProtection::Fingerprint(
    const PacketContext& context)
{
    return Fnv1a64(context.payload.bytes);
}

uint64_t
SpatemReplayProtection::SemanticTimestamp(
    const SpatemIntersection& intersection)
{
    constexpr uint64_t kMillisecondsPerMinute =
        60ULL * 1000ULL;

    return static_cast<uint64_t>(
               *intersection.minuteOfYear) *
               kMillisecondsPerMinute +
           static_cast<uint64_t>(
               *intersection.dSecond);
}

bool
SpatemReplayProtection::HasSemanticTimestamp(
    const SpatemIntersection& intersection)
{
    return intersection.minuteOfYear.has_value() &&
           intersection.dSecond.has_value();
}

} // namespace ns3
