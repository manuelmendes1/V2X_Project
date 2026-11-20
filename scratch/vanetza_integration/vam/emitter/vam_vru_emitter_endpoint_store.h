#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "vanetza_integration/vam/emitter/vam_vru_emitter_endpoint.h"

namespace ns3
{

// Owns VAM VRU network endpoints by active SUMO person and exact presence epoch.
// A retired endpoint remains alive while the scheduler still owns runtime state
// for that presence. Logical ITS identity remains simulation-scoped and is not
// released by this store.
class VamVruEmitterEndpointStore
{
public:
    using PresenceKey = std::pair<std::string, std::uint64_t>;

    bool InsertActive(std::unique_ptr<VamVruEmitterEndpoint> endpoint)
    {
        if (!endpoint || endpoint->PersonId().empty())
        {
            return false;
        }

        const std::string personId = endpoint->PersonId();
        const auto [it, inserted] =
            m_active.emplace(personId, std::move(endpoint));
        (void)it;
        return inserted;
    }

    // Move the active endpoint to the retired ledger only when its exact
    // presence still has scheduler-owned runtime state. Otherwise destruction
    // unregisters its IPv4 mapping immediately.
    bool RetireActive(const std::string& personId, bool retainRuntime)
    {
        if (personId.empty())
        {
            return false;
        }

        const auto activeIt = m_active.find(personId);
        if (activeIt == m_active.end() || !activeIt->second)
        {
            return false;
        }

        if (retainRuntime)
        {
            const PresenceKey key{
                personId, activeIt->second->PresenceEpoch().value};
            if (m_retired.find(key) != m_retired.end())
            {
                return false;
            }

            m_retired.emplace(key, std::move(activeIt->second));
        }

        m_active.erase(activeIt);
        return true;
    }

    // Reconcile an UPSERT against the endpoint's recorded presence epoch.
    // Same-epoch UPSERTs preserve the existing endpoint. A different epoch
    // retires or destroys the previous endpoint according to retainOldRuntime;
    // the caller then creates and inserts the new active endpoint.
    bool ReconcileActivePresence(
        const std::string& personId,
        vanetza_integration::vam::VamPersonPresenceEpoch currentEpoch,
        bool retainOldRuntime)
    {
        if (personId.empty())
        {
            return false;
        }

        const auto activeIt = m_active.find(personId);
        if (activeIt == m_active.end())
        {
            return true;
        }

        if (!activeIt->second)
        {
            return false;
        }

        if (activeIt->second->PresenceEpoch() == currentEpoch)
        {
            return true;
        }

        return RetireActive(personId, retainOldRuntime);
    }

    VamVruEmitterEndpoint* FindActive(const std::string& personId) noexcept
    {
        const auto it = m_active.find(personId);
        return it == m_active.end() ? nullptr : it->second.get();
    }

    const VamVruEmitterEndpoint* FindActive(
        const std::string& personId) const noexcept
    {
        const auto it = m_active.find(personId);
        return it == m_active.end() ? nullptr : it->second.get();
    }

    VamVruEmitterEndpoint* FindRetired(
        const std::string& personId,
        vanetza_integration::vam::VamPersonPresenceEpoch presenceEpoch) noexcept
    {
        const auto it = m_retired.find(PresenceKey{personId, presenceEpoch.value});
        return it == m_retired.end() ? nullptr : it->second.get();
    }

    const VamVruEmitterEndpoint* FindRetired(
        const std::string& personId,
        vanetza_integration::vam::VamPersonPresenceEpoch presenceEpoch) const noexcept
    {
        const auto it = m_retired.find(PresenceKey{personId, presenceEpoch.value});
        return it == m_retired.end() ? nullptr : it->second.get();
    }

    std::size_t ActiveCount() const noexcept
    {
        return m_active.size();
    }

    std::size_t RetiredCount() const noexcept
    {
        return m_retired.size();
    }

    // Keep an endpoint while the predicate says the scheduler still retains
    // the corresponding presence. Return the number of endpoints released.
    template <typename RetainPredicate>
    std::size_t PruneRetired(RetainPredicate&& shouldRetain)
    {
        std::size_t released = 0;
        for (auto it = m_retired.begin(); it != m_retired.end();)
        {
            const auto& key = it->first;
            const auto epoch =
                vanetza_integration::vam::VamPersonPresenceEpoch{key.second};
            if (shouldRetain(key.first, epoch))
            {
                ++it;
            }
            else
            {
                it = m_retired.erase(it);
                ++released;
            }
        }
        return released;
    }

    void ClearAll() noexcept
    {
        m_active.clear();
        m_retired.clear();
    }

private:
    std::map<std::string, std::unique_ptr<VamVruEmitterEndpoint>> m_active;
    std::map<PresenceKey, std::unique_ptr<VamVruEmitterEndpoint>> m_retired;
};

} // namespace ns3
