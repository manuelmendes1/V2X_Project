#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "vanetza_integration/denm/types/denm_input.h"

namespace ns3
{

struct DenmActionIdHash
{
    std::size_t operator()(
        const DenmActionId& actionId) const noexcept
    {
        const std::size_t stationHash =
            std::hash<uint32_t>{}(
                actionId.originatingStationId);

        const std::size_t sequenceHash =
            std::hash<uint16_t>{}(
                actionId.sequenceNumber);

        return stationHash ^
               (sequenceHash + 0x9e3779b9u +
                (stationHash << 6) +
                (stationHash >> 2));
    }
};

struct DenmActionIdEqual
{
    bool operator()(
        const DenmActionId& lhs,
        const DenmActionId& rhs) const noexcept
    {
        return lhs.originatingStationId ==
                   rhs.originatingStationId &&
               lhs.sequenceNumber ==
                   rhs.sequenceNumber;
    }
};

struct DenmReplayEntry
{
    DenmManagementAction lastAction =
        DenmManagementAction::NEW;

    uint64_t lastReceptionTimestamp = 0;

    std::unordered_set<uint64_t> fingerprints;
};

struct DenmReplayContext
{
    std::unordered_map<
        DenmActionId,
        DenmReplayEntry,
        DenmActionIdHash,
        DenmActionIdEqual>
        database;
};

} // namespace ns3
