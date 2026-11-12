#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace ns3
{

enum class ItsStationIdentityOwnerKind : std::uint8_t
{
    Vehicle,
    ItsStation
};

struct ItsStationIdentityOwner
{
    ItsStationIdentityOwnerKind kind =
        ItsStationIdentityOwnerKind::Vehicle;

    std::string logicalId;

    bool operator<(
        const ItsStationIdentityOwner& other) const noexcept
    {
        if (kind != other.kind)
        {
            return kind < other.kind;
        }

        return logicalId < other.logicalId;
    }

    bool operator==(
        const ItsStationIdentityOwner& other) const noexcept = default;
};

// Simulation-scoped authority for globally unique ITS station identifiers.
//
// Logical reservations are independent of endpoint/network lifetimes.
// Destroying an endpoint must not implicitly release its logical identity.
class ItsStationIdentityAuthority
{
public:
    // The simulation uses exactly one process-wide identity authority.
    static ItsStationIdentityAuthority& Instance() noexcept;

    // Reserve a caller-provided stationId for one logical owner.
    // Repeating the same owner/ID pair is idempotent.
    bool Reserve(
        const ItsStationIdentityOwner& owner,
        std::uint32_t stationId);

    // Reserve nodeId + 1, rejecting zero/overflow and global collisions.
    // The output argument is only modified after successful reservation.
    bool ReserveNodeDerived(
        const ItsStationIdentityOwner& owner,
        std::uint32_t nodeId,
        std::uint32_t& stationId);

    std::optional<std::uint32_t> FindStationId(
        const ItsStationIdentityOwner& owner) const;

    bool IsReservedBy(
        std::uint32_t stationId,
        const ItsStationIdentityOwner& owner) const;

    // Release only when both owner and stationId match the reservation.
    bool Release(
        const ItsStationIdentityOwner& owner,
        std::uint32_t stationId);

    // End-of-simulation cleanup.
    void Clear() noexcept;

    std::size_t Size() const noexcept;

private:
    ItsStationIdentityAuthority() = default;
    ItsStationIdentityAuthority(const ItsStationIdentityAuthority&) = delete;
    ItsStationIdentityAuthority& operator=(const ItsStationIdentityAuthority&) = delete;
    ItsStationIdentityAuthority(ItsStationIdentityAuthority&&) = delete;
    ItsStationIdentityAuthority& operator=(ItsStationIdentityAuthority&&) = delete;

    static bool IsValidOwner(
        const ItsStationIdentityOwner& owner) noexcept;

    // The two indexes must remain consistent.
    std::map<ItsStationIdentityOwner, std::uint32_t> m_stationIdByOwner;
    std::map<std::uint32_t, ItsStationIdentityOwner> m_ownerByStationId;
};

} // namespace ns3
