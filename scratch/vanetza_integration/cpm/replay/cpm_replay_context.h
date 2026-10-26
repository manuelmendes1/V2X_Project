#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

namespace ns3
{

// Bounded exact-payload duplicate detection for the initial CPM RX baseline.
// This is not a replacement for the secured replay policy in Etapa 10.10.
struct CpmReplayContext
{
    static constexpr std::size_t kMaximumFingerprints = 1024;

    std::unordered_set<std::string> fingerprints;
    std::deque<std::string> insertionOrder;

    bool AcceptIfNew(
        std::uint32_t stationId,
        const std::vector<std::uint8_t>& payload)
    {
        if (payload.empty())
        {
            return false;
        }

        std::string key(sizeof(stationId), '\0');
        std::memcpy(key.data(), &stationId, sizeof(stationId));
        key.append(
            reinterpret_cast<const char*>(payload.data()),
            payload.size());

        if (fingerprints.find(key) != fingerprints.end())
        {
            return false;
        }

        if (insertionOrder.size() >= kMaximumFingerprints)
        {
            fingerprints.erase(insertionOrder.front());
            insertionOrder.pop_front();
        }

        fingerprints.insert(key);
        insertionOrder.push_back(std::move(key));
        return true;
    }
};

} // namespace ns3
