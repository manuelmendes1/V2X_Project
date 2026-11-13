#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace ns3
{

struct SumoVehicleLifecycleState
{
    bool active{};

    // Unknown when the first accepted sample belonged to a legacy
    // timestamp-less datagram. Never infer this value retrospectively.
    std::optional<std::uint64_t> firstObservedAtUs;

    // Timestamp of the latest accepted event carrying logical SUMO time.
    // Timestamp-less updates preserve this value.
    std::optional<std::uint64_t> lastEventTimeUs;
};

// Tracks SUMO presence independently of the lifetime and ownership of
// ns-3 VehicleState, Node, Socket and VanetzaRuntime objects.
class SumoVehicleLifecycleRegistry
{
public:
    static SumoVehicleLifecycleRegistry& Instance();

    bool ApplyUpsert(
        const std::string& vehId,
        std::optional<std::uint64_t> simulationTimeUs);

    bool ApplyRemove(
        const std::string& vehId,
        std::uint64_t simulationTimeUs);

    std::optional<SumoVehicleLifecycleState> Find(
        const std::string& vehId) const;

    // Untracked vehicles remain eligible for compatibility with ITS-S
    // instances that are not managed by the SUMO lifecycle source.
    bool IsDispatchEligible(const std::string& vehId) const;

    void Clear() noexcept;

private:
    std::unordered_map<std::string, SumoVehicleLifecycleState> m_vehicles;
};

} // namespace ns3
