#pragma once

#include "vanetza_integration/denm/trigger/denm_trigger.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ns3
{

struct DenmScenarioEvent
{
    std::string vehicleId;
    std::string eventId;

    DenmTriggerSource source =
        DenmTriggerSource::SCENARIO;

    DenmTriggerEventType eventType =
        DenmTriggerEventType::HAZARD_DETECTED;

    uint64_t triggerTimeUs = 0;

    double x = 0.0;
    double y = 0.0;

    uint32_t causeCode = 0;
    double relevanceDistanceMeters = 0.0;
    uint64_t validityDurationMs = 0;

    std::string reason;
};

class DenmScenarioEventSource
{
  public:
    using SubmissionTimingCallback =
        std::function<void(const DenmScenarioEvent& event,
                           uint64_t triggerTimestampUs,
                           uint64_t submissionTimestampUs,
                           uint64_t delayUs)>;

    bool AddEvent(const DenmScenarioEvent& event);

    std::size_t Evaluate(uint64_t nowUs);

    void SetSubmissionTimingCallback(SubmissionTimingCallback callback);

    std::size_t EventCount() const noexcept;
    std::size_t PendingEventCount() const noexcept;

    void Clear() noexcept;

  private:
    struct ScheduledScenarioEvent
    {
        DenmScenarioEvent event;
        bool submitted = false;
    };

    std::vector<ScheduledScenarioEvent> m_events;
    SubmissionTimingCallback m_submissionTimingCallback;
};

} // namespace ns3
