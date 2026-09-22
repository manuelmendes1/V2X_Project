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

    DenmMessage effectiveMessage = message;

    // ------------------------------------------------------------
    // Implicit UPDATE classification
    //
    // On the wire, R1 DENM uses:
    //   - absent termination for NEW and UPDATE
    //   - same ActionID for the whole lifecycle
    //
    // The ASN.1 decoder therefore initially represents both NEW and
    // UPDATE as DenmManagementAction::NEW. A previously known,
    // non-terminal ActionID with a new application payload is an UPDATE.
    // ------------------------------------------------------------

    auto existing =
        replay.database.find(
            effectiveMessage.actionId);

    if (effectiveMessage.action ==
            DenmManagementAction::NEW &&
        existing != replay.database.end())
    {
        const uint64_t fingerprint =
            Fingerprint(ctx);

        const bool samePayload =
            existing->second.fingerprints.find(
                fingerprint) !=
            existing->second.fingerprints.end();

        const bool terminal =
            IsTerminal(
                existing->second.lastAction);

        if (!samePayload && !terminal)
        {
            effectiveMessage.action =
                DenmManagementAction::UPDATE;
        }
    }

    if (IsReplay(
            ctx,
            effectiveMessage,
            replay))
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
                    effectiveMessage.actionId.originatingStationId) +
            " action_seq=" +
                std::to_string(
                    effectiveMessage.actionId.sequenceNumber));

        return false;
    }

    if (!Apply(
            ctx,
            effectiveMessage,
            replay))
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
                effectiveMessage.actionId.originatingStationId) +
        " action_seq=" +
            std::to_string(
                effectiveMessage.actionId.sequenceNumber) +
        " action=" +
            std::to_string(
                static_cast<unsigned>(
                    effectiveMessage.action)));

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
     * A terminal action cannot be followed by UPDATE,
     * CANCELLATION or NEGATION.
     *
     * For a non-terminal ActionID, a NEW decoded from the wire may
     * represent an implicit UPDATE. That classification is performed
     * by Process() before reaching this function.
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
