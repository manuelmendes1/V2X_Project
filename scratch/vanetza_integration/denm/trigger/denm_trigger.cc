#include "denm_trigger.h"

#include <cmath>
#include <utility>

namespace ns3
{

DenmTrigger::DenmTrigger(TriggerCallback callback)
    : m_callback(std::move(callback))
{
}

bool
DenmTrigger::Submit(const DenmTriggerRequest& request)
{
    std::string error;

    if (!Validate(request, error))
    {
        return false;
    }

    if (!m_callback)
    {
        return false;
    }

    m_callback(request);
    return true;
}

bool
DenmTrigger::Validate(const DenmTriggerRequest& request, std::string& error)
{
    if (!std::isfinite(request.x) || !std::isfinite(request.y))
    {
        error = "invalid_event_position";
        return false;
    }

    if (request.causeCode == 0)
    {
        error = "missing_cause_code";
        return false;
    }

    if (request.triggerTimestampUs == 0)
    {
        error = "missing_trigger_timestamp";
        return false;
    }

    if (request.eventType != DenmTriggerEventType::HAZARD_CLEARED &&
        request.validityDurationMs == 0)
    {
        error = "missing_validity_duration";
        return false;
    }

    if (request.relevanceDistanceMeters < 0.0 ||
        !std::isfinite(request.relevanceDistanceMeters))
    {
        error = "invalid_relevance_distance";
        return false;
    }

    return true;
}

} // namespace ns3
