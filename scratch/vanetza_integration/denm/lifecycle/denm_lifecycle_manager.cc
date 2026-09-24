#include "denm_lifecycle_manager.h"

#include "vanetza_integration/denm/action/denm_action_id_manager.h"


namespace ns3
{

bool
DenmLifecycleManager::Process(
    VehicleState& vehicle,
    const DenmTriggerRequest& request,
    DenmLifecycleResult& result)
{
    result = {};

    const std::string& vehicleId =
        vehicle.identity.vehId;

    const uint32_t stationId =
        vehicle.identity.stationId;

    if (vehicleId.empty() ||
        stationId == 0)
    {
        return false;
    }

    if (request.vehicleId.empty() ||
        request.eventId.empty())
    {
        return false;
    }

    // The application trigger must identify the same vehicle
    // represented by the authoritative VehicleState.
    if (request.vehicleId != vehicleId)
    {
        return false;
    }

    const DenmEventId key{
        request.vehicleId,
        request.eventId};

    auto it =
        m_events.find(key);

    switch (request.eventType)
    {
        case DenmTriggerEventType::HAZARD_DETECTED:
        {
            if (it != m_events.end())
            {
                return false;
            }

            const DenmActionId actionId =
                DenmActionIdManager::Instance().StartAction(
                    stationId);

            if (actionId.sequenceNumber == 0)
            {
                return false;
            }

            m_events.emplace(
                key,
                EventState{
                    actionId,
                    DenmManagementAction::NEW,
                    stationId});

            result.action =
                DenmManagementAction::NEW;

            result.actionId =
                actionId;

            return true;
        }

        case DenmTriggerEventType::HAZARD_UPDATED:
        {
            if (it == m_events.end())
            {
                return false;
            }

            if (it->second.originatingStationId !=
                stationId)
            {
                return false;
            }

            const DenmActionId actionId =
                it->second.actionId;

            if (!DenmActionIdManager::Instance().CanApply(
                    actionId,
                    DenmManagementAction::UPDATE))
            {
                return false;
            }

            if (!DenmActionIdManager::Instance().Apply(
                    actionId,
                    DenmManagementAction::UPDATE))
            {
                return false;
            }

            it->second.lastAction =
                DenmManagementAction::UPDATE;

            result.action =
                DenmManagementAction::UPDATE;

            result.actionId =
                actionId;

            return true;
        }

        case DenmTriggerEventType::HAZARD_CLEARED:
        {
            if (it == m_events.end())
            {
                return false;
            }

            if (it->second.originatingStationId !=
                stationId)
            {
                return false;
            }

            const DenmActionId actionId =
                it->second.actionId;

            if (!DenmActionIdManager::Instance().CanApply(
                    actionId,
                    DenmManagementAction::CANCELLATION))
            {
                return false;
            }

            if (!DenmActionIdManager::Instance().Apply(
                    actionId,
                    DenmManagementAction::CANCELLATION))
            {
                return false;
            }

            result.action =
                DenmManagementAction::CANCELLATION;

            result.actionId =
                actionId;

            m_events.erase(it);

            return true;
        }
    }

    return false;
}


bool
DenmLifecycleManager::Repeat(
    VehicleState& vehicle,
    const std::string& eventId,
    DenmLifecycleResult& result)
{
    result = {};

    const std::string& vehicleId =
        vehicle.identity.vehId;

    const uint32_t stationId =
        vehicle.identity.stationId;

    if (vehicleId.empty() ||
        stationId == 0 ||
        eventId.empty())
    {
        return false;
    }

    const DenmEventId key{
        vehicleId,
        eventId};

    const auto it =
        m_events.find(key);

    if (it == m_events.end())
    {
        return false;
    }

    if (it->second.originatingStationId !=
        stationId)
    {
        return false;
    }

    if (!DenmActionIdManager::Instance().IsActive(
            it->second.actionId))
    {
        return false;
    }

    // Repetition does not create or mutate lifecycle state.
    // It reuses the current event and its existing ActionID.
    result.action =
        it->second.lastAction;

    result.actionId =
        it->second.actionId;

    return true;
}


bool
DenmLifecycleManager::Abort(
    const std::string& vehicleId,
    const std::string& eventId)
{
    const DenmEventId key{
        vehicleId,
        eventId};

    const auto it =
        m_events.find(key);

    if (it == m_events.end())
    {
        return false;
    }

    const DenmActionId actionId =
        it->second.actionId;

    if (DenmActionIdManager::Instance().IsActive(actionId))
    {
        DenmActionIdManager::Instance().Apply(
            actionId,
            DenmManagementAction::CANCELLATION);
    }

    m_events.erase(it);

    return true;
}

bool
DenmLifecycleManager::GetActionId(
    const std::string& vehicleId,
    const std::string& eventId,
    DenmActionId& actionId) const
{
    const DenmEventId key{
        vehicleId,
        eventId};

    const auto it =
        m_events.find(key);

    if (it == m_events.end())
    {
        return false;
    }

    actionId =
        it->second.actionId;

    return true;
}

void DenmLifecycleManager::Reset()
{
    m_events.clear();

    // ActionID state belongs to the originator lifecycle authority.
    // Resetting the lifecycle must therefore also clear its ActionIDs.
    DenmActionIdManager::Instance().Reset();
}

} // namespace ns3
