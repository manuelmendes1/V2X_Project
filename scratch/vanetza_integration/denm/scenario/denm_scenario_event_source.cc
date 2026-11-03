#include "denm_scenario_event_source.h"

#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"

namespace ns3
{

void
DenmScenarioEventSource::SetSubmissionTimingCallback(
    SubmissionTimingCallback callback)
{
    m_submissionTimingCallback = callback;
}

bool
DenmScenarioEventSource::AddEvent(const DenmScenarioEvent& event)
{
    if (event.vehicleId.empty() ||
        event.eventId.empty() ||
        event.triggerTimeUs == 0)
    {
        return false;
    }

    m_events.push_back(
        ScheduledScenarioEvent{event, false});

    return true;
}

std::size_t
DenmScenarioEventSource::Evaluate(uint64_t nowUs)
{
    std::size_t submittedCount = 0;

    for (auto& scheduled : m_events)
    {
        if (scheduled.submitted)
        {
            continue;
        }

        const bool isSumoEvent =
            scheduled.event.source ==
            DenmTriggerSource::SUMO;

        if (!isSumoEvent &&
            nowUs < scheduled.event.triggerTimeUs)
        {
            continue;
        }

        DenmTriggerRequest request;

        request.source = scheduled.event.source;
        request.eventType = scheduled.event.eventType;

        request.vehicleId = scheduled.event.vehicleId;
        request.eventId = scheduled.event.eventId;

        request.x = scheduled.event.x;
        request.y = scheduled.event.y;

        request.causeCode = scheduled.event.causeCode;
        request.relevanceDistanceMeters =
            scheduled.event.relevanceDistanceMeters;
        request.validityDurationMs =
            scheduled.event.validityDurationMs;

        request.triggerTimestampUs =
            scheduled.event.triggerTimeUs;

        request.reason = scheduled.event.reason;

        if (DenmScheduler::Submit(request))
        {
            scheduled.submitted = true;
            ++submittedCount;

            if (m_submissionTimingCallback)
            {
                const uint64_t delayUs =
                    nowUs >= scheduled.event.triggerTimeUs
                        ? nowUs - scheduled.event.triggerTimeUs
                        : 0;

                m_submissionTimingCallback(
                    scheduled.event,
                    scheduled.event.triggerTimeUs,
                    nowUs,
                    delayUs);
            }
        }
    }

    return submittedCount;
}

std::size_t
DenmScenarioEventSource::EventCount() const noexcept
{
    return m_events.size();
}

std::size_t
DenmScenarioEventSource::PendingEventCount() const noexcept
{
    std::size_t count = 0;

    for (const auto& scheduled : m_events)
    {
        if (!scheduled.submitted)
        {
            ++count;
        }
    }

    return count;
}

void
DenmScenarioEventSource::Clear() noexcept
{
    m_events.clear();
}

} // namespace ns3
