#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"

namespace ns3
{

SumoVehicleLifecycleRegistry&
SumoVehicleLifecycleRegistry::Instance()
{
    static SumoVehicleLifecycleRegistry registry;
    return registry;
}

bool
SumoVehicleLifecycleRegistry::ApplyUpsert(
    const std::string& vehId,
    std::optional<std::uint64_t> simulationTimeUs)
{
    if (vehId.empty())
    {
        return false;
    }

    const auto found = m_vehicles.find(vehId);

    if (found != m_vehicles.end())
    {
        const auto& current = found->second;

        // An inactive identity can only re-enter with a timestamp that
        // proves the UPSERT is newer than its REMOVE tombstone.
        if (!current.active)
        {
            if (!simulationTimeUs.has_value() ||
                !current.lastEventTimeUs.has_value() ||
                *simulationTimeUs <= *current.lastEventTimeUs)
            {
                return false;
            }

            m_vehicles.insert_or_assign(
                vehId,
                SumoVehicleLifecycleState{
                    true,
                    simulationTimeUs,
                    simulationTimeUs});
            return true;
        }

        // A timestamped event must advance the known event time strictly.
        if (simulationTimeUs.has_value() &&
            current.lastEventTimeUs.has_value() &&
            *simulationTimeUs <= *current.lastEventTimeUs)
        {
            return false;
        }

        auto updated = current;

        if (simulationTimeUs.has_value())
        {
            updated.lastEventTimeUs = simulationTimeUs;
        }

        m_vehicles.insert_or_assign(vehId, updated);
        return true;
    }

    m_vehicles.emplace(
        vehId,
        SumoVehicleLifecycleState{
            true,
            simulationTimeUs,
            simulationTimeUs});

    return true;
}

bool
SumoVehicleLifecycleRegistry::ApplyRemove(
    const std::string& vehId,
    std::uint64_t simulationTimeUs)
{
    if (vehId.empty())
    {
        return false;
    }

    const auto found = m_vehicles.find(vehId);

    if (found != m_vehicles.end() &&
        found->second.lastEventTimeUs.has_value() &&
        simulationTimeUs <= *found->second.lastEventTimeUs)
    {
        return false;
    }

    SumoVehicleLifecycleState removedState{};

    if (found != m_vehicles.end())
    {
        removedState = found->second;
    }

    removedState.active = false;
    removedState.lastEventTimeUs = simulationTimeUs;

    m_vehicles.insert_or_assign(vehId, removedState);
    return true;
}

std::optional<SumoVehicleLifecycleState>
SumoVehicleLifecycleRegistry::Find(const std::string& vehId) const
{
    const auto found = m_vehicles.find(vehId);
    if (found == m_vehicles.end())
    {
        return std::nullopt;
    }

    return found->second;
}

bool
SumoVehicleLifecycleRegistry::IsDispatchEligible(
    const std::string& vehId) const
{
    if (vehId.empty())
    {
        return false;
    }

    const auto found = m_vehicles.find(vehId);
    return found == m_vehicles.end() || found->second.active;
}

void
SumoVehicleLifecycleRegistry::Clear() noexcept
{
    m_vehicles.clear();
}

} // namespace ns3
