#include "denm_replay_protection.h"

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

bool
IsTerminal(DenmManagementAction action)
{
    return action == DenmManagementAction::CANCELLATION ||
           action == DenmManagementAction::NEGATION;
}

} // namespace

namespace ns3
{

bool
DenmReplayProtection::Process(
    PacketContext& ctx,
    const DenmMessage& message,
    DenmReplayContext& replay)
{
    if (message.actionId.originatingStationId == 0 ||
        message.actionId.sequenceNumber == 0)
    {
        ctx.MarkFailure();
        ctx.reason = "invalid_denm_action_id";

        V2XWriteLog(
            ctx,
            V2XLogLevel::WARN,
            "DENM",
            "DENM_REPLAY_DROP",
            "invalid ActionID");

        return false;
    }

    if (IsReplay(ctx, message, replay))
    {
        ctx.MarkDrop();
        ctx.reason = "denm_replay_detected";

        V2XWriteLog(
            ctx,
            V2XLogLevel::WARN,
            "DENM",
            "DENM_REPLAY_DROP",
            "origin_station=" +
                std::to_string(
                    message.actionId.originatingStationId) +
            " action_seq=" +
                std::to_string(
                    message.actionId.sequenceNumber));

        return false;
    }

    if (!Apply(ctx, message, replay))
    {
        ctx.MarkFailure();
        ctx.reason = "invalid_denm_lifecycle_transition";

        V2XWriteLog(
            ctx,
            V2XLogLevel::WARN,
            "DENM",
            "DENM_REPLAY_FAIL",
            "invalid lifecycle transition");

        return false;
    }

    V2XWriteLog(
        ctx,
        V2XLogLevel::INFO,
        "DENM",
        "DENM_REPLAY_OK",
        "origin_station=" +
            std::to_string(
                message.actionId.originatingStationId) +
        " action_seq=" +
            std::to_string(
                message.actionId.sequenceNumber));

    return true;
}

bool
DenmReplayProtection::IsReplay(
    const PacketContext& ctx,
    const DenmMessage& message,
    const DenmReplayContext& replay)
{
    auto it =
        replay.database.find(message.actionId);

    if (it == replay.database.end())
    {
        return false;
    }

    const DenmReplayEntry& entry =
        it->second;

    const uint64_t fingerprint =
        Fingerprint(ctx);

    /*
     * A repeated ActionID + identical application payload
     * is a replay.
     */
    if (entry.fingerprints.find(fingerprint) !=
        entry.fingerprints.end())
    {
        return true;
    }

    /*
     * NEW must never be accepted twice for the same ActionID.
     */
    if (message.action == DenmManagementAction::NEW)
    {
        return true;
    }

    /*
     * A terminal action cannot be followed by UPDATE,
     * CANCELLATION or NEGATION.
     */
    if (IsTerminal(entry.lastAction))
    {
        return true;
    }

    return false;
}

bool
DenmReplayProtection::Apply(
    const PacketContext& ctx,
    const DenmMessage& message,
    DenmReplayContext& replay)
{
    DenmReplayEntry& entry =
        replay.database[message.actionId];

    const uint64_t fingerprint =
        Fingerprint(ctx);

    switch (message.action)
    {
        case DenmManagementAction::NEW:
            if (!entry.fingerprints.empty())
            {
                return false;
            }

            entry.lastAction =
                DenmManagementAction::NEW;

            entry.lastReceptionTimestamp =
                ctx.timestamp;

            entry.fingerprints.insert(
                fingerprint);

            return true;

        case DenmManagementAction::UPDATE:
            if (entry.fingerprints.empty())
            {
                return false;
            }

            if (IsTerminal(entry.lastAction))
            {
                return false;
            }

            entry.lastAction =
                DenmManagementAction::UPDATE;

            entry.lastReceptionTimestamp =
                ctx.timestamp;

            entry.fingerprints.insert(
                fingerprint);

            return true;

        case DenmManagementAction::CANCELLATION:
        case DenmManagementAction::NEGATION:
            if (entry.fingerprints.empty())
            {
                return false;
            }

            if (IsTerminal(entry.lastAction))
            {
                return false;
            }

            entry.lastAction =
                message.action;

            entry.lastReceptionTimestamp =
                ctx.timestamp;

            entry.fingerprints.insert(
                fingerprint);

            return true;
    }

    return false;
}

uint64_t
DenmReplayProtection::Fingerprint(
    const PacketContext& ctx)
{
    return Fnv1a64(
        ctx.payload.bytes);
}

} // namespace ns3
