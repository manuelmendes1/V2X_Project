#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/denm/types/denm_input.h"

namespace ns3
{

struct DenmLifecycleResult
{
    DenmManagementAction action =
        DenmManagementAction::NEW;

    DenmActionId actionId;
};

class DenmLifecycleManager
{
public:
    bool Process(
        VehicleState& vehicle,
        const DenmTriggerRequest& request,
        DenmLifecycleResult& result);

    // Roll back local lifecycle state after a failed generation/transmission.
    // This is internal failure cleanup, not a wire-level CANCELLATION event.
    bool Abort(
        const std::string& vehicleId,
        const std::string& eventId);

    bool GetActionId(
        const std::string& vehicleId,
        const std::string& eventId,
        DenmActionId& actionId) const;

    void Reset();

private:
    struct EventKey
    {
        std::string vehicleId;
        std::string eventId;

        bool operator==(const EventKey& other) const noexcept
        {
            return vehicleId == other.vehicleId &&
                   eventId == other.eventId;
        }
    };

    struct EventKeyHash
    {
        std::size_t operator()(
            const EventKey& key) const noexcept;
    };

    struct EventState
    {
        DenmActionId actionId;
        DenmManagementAction lastAction =
            DenmManagementAction::NEW;
        uint32_t originatingStationId = 0;
    };

    std::unordered_map<
        EventKey,
        EventState,
        EventKeyHash>
        m_events;
};

} // namespace ns3
