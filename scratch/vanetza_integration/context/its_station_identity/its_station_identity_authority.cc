#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"

#include <limits>
#include <utility>

namespace ns3
{

ItsStationIdentityAuthority&
ItsStationIdentityAuthority::Instance() noexcept
{
    static ItsStationIdentityAuthority instance;
    return instance;
}

bool
ItsStationIdentityAuthority::IsValidOwner(
    const ItsStationIdentityOwner& owner) noexcept
{
    if (owner.logicalId.empty())
    {
        return false;
    }

    switch (owner.kind)
    {
    case ItsStationIdentityOwnerKind::Vehicle:
    case ItsStationIdentityOwnerKind::ItsStation:
        return true;
    }

    return false;
}

bool
ItsStationIdentityAuthority::Reserve(
    const ItsStationIdentityOwner& owner,
    std::uint32_t stationId)
{
    if (!IsValidOwner(owner) || stationId == 0)
    {
        return false;
    }

    const auto ownerIt = m_stationIdByOwner.find(owner);

    if (ownerIt != m_stationIdByOwner.end())
    {
        // The same logical owner cannot silently change its ITS identity.
        return ownerIt->second == stationId;
    }

    const auto stationIt = m_ownerByStationId.find(stationId);

    if (stationIt != m_ownerByStationId.end())
    {
        // stationId is globally unique across all owner kinds.
        return false;
    }

    const auto [insertedOwnerIt, ownerInserted] =
        m_stationIdByOwner.emplace(owner, stationId);

    if (!ownerInserted)
    {
        return false;
    }

    try
    {
        const auto [insertedStationIt, stationInserted] =
            m_ownerByStationId.emplace(stationId, owner);

        (void)insertedStationIt;

        if (!stationInserted)
        {
            m_stationIdByOwner.erase(insertedOwnerIt);
            return false;
        }
    }
    catch (...)
    {
        // Preserve the two-index invariant if the second insertion fails.
        m_stationIdByOwner.erase(insertedOwnerIt);
        throw;
    }

    return true;
}

bool
ItsStationIdentityAuthority::ReserveNodeDerived(
    const ItsStationIdentityOwner& owner,
    std::uint32_t nodeId,
    std::uint32_t& stationId)
{
    if (nodeId == std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    const std::uint32_t candidate = nodeId + 1U;

    if (!Reserve(owner, candidate))
    {
        return false;
    }

    stationId = candidate;
    return true;
}

std::optional<std::uint32_t>
ItsStationIdentityAuthority::FindStationId(
    const ItsStationIdentityOwner& owner) const
{
    const auto it = m_stationIdByOwner.find(owner);

    if (it == m_stationIdByOwner.end())
    {
        return std::nullopt;
    }

    return it->second;
}

bool
ItsStationIdentityAuthority::IsReservedBy(
    std::uint32_t stationId,
    const ItsStationIdentityOwner& owner) const
{
    if (!IsValidOwner(owner) || stationId == 0)
    {
        return false;
    }

    const auto it = m_ownerByStationId.find(stationId);

    return it != m_ownerByStationId.end() &&
           it->second == owner;
}

bool
ItsStationIdentityAuthority::Release(
    const ItsStationIdentityOwner& owner,
    std::uint32_t stationId)
{
    if (!IsValidOwner(owner) || stationId == 0)
    {
        return false;
    }

    const auto ownerIt = m_stationIdByOwner.find(owner);

    if (ownerIt == m_stationIdByOwner.end() ||
        ownerIt->second != stationId)
    {
        return false;
    }

    const auto stationIt = m_ownerByStationId.find(stationId);

    if (stationIt == m_ownerByStationId.end() ||
        !(stationIt->second == owner))
    {
        return false;
    }

    m_ownerByStationId.erase(stationIt);
    m_stationIdByOwner.erase(ownerIt);

    return true;
}

void
ItsStationIdentityAuthority::Clear() noexcept
{
    m_stationIdByOwner.clear();
    m_ownerByStationId.clear();
}

std::size_t
ItsStationIdentityAuthority::Size() const noexcept
{
    return m_stationIdByOwner.size();
}

} // namespace ns3
