#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"
#include "vanetza_integration/denm/scenario/sumo_denm_event_parser.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler_event_store.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace ns3
{

std::vector<DenmTriggerRequest> g_testSubmittedRequests;

bool
DenmScheduler::Submit(const DenmTriggerRequest& request)
{
    g_testSubmittedRequests.push_back(request);
    return true;
}

} // namespace ns3

namespace
{

int g_failures = 0;

void
Check(bool condition, const std::string& name)
{
    if (condition)
    {
        std::cout << "[PASS] " << name << '\n';
    }
    else
    {
        std::cout << "[FAIL] " << name << '\n';
        ++g_failures;
    }
}

struct TimingRecord
{
    uint64_t triggerUs = 0;
    uint64_t submissionUs = 0;
    uint64_t delayUs = 0;
};

void
TestSumoParserPreservesImmediateOrigin()
{
    const auto event = ns3::SumoDenmEventParser::Parse(
        "DENM_EVENT,veh0,sumo-future,DETECTED,"
        "10.5,20.5,94,100,5000,4510000,sumo-origin");

    Check(event.has_value(),
          "DENM_EVENT is accepted");

    Check(event.has_value() &&
              event->source == ns3::DenmTriggerSource::SUMO,
          "DENM_EVENT keeps SUMO source");
}

void
TestScenarioParserAcceptsScheduledOrigin()
{
    const auto event = ns3::SumoDenmEventParser::Parse(
        "DENM_SCENARIO_EVENT,veh0,scenario-future,DETECTED,"
        "10.5,20.5,94,100,5000,4510000,scenario-origin");

    Check(event.has_value(),
          "DENM_SCENARIO_EVENT is accepted");

    Check(event.has_value() &&
              event->source == ns3::DenmTriggerSource::SCENARIO,
          "DENM_SCENARIO_EVENT has SCENARIO source");

    Check(event.has_value() &&
              event->triggerTimeUs == 4510000,
          "scenario trigger timestamp is preserved");
}

void
TestScenarioEventWaitsUntilTriggerTime()
{
    ns3::g_testSubmittedRequests.clear();

    ns3::DenmScenarioEvent event;
    event.source = ns3::DenmTriggerSource::SCENARIO;
    event.eventType = ns3::DenmTriggerEventType::HAZARD_DETECTED;
    event.vehicleId = "veh0";
    event.eventId = "scenario-scheduled";
    event.triggerTimeUs = 4510000;
    event.x = 10.5;
    event.y = 20.5;
    event.causeCode = 94;
    event.relevanceDistanceMeters = 100.0;
    event.validityDurationMs = 5000;
    event.reason = "scenario-origin";

    ns3::DenmScenarioEventSource source;

    std::vector<TimingRecord> timingRecords;

    source.SetSubmissionTimingCallback(
        [&timingRecords](const ns3::DenmScenarioEvent&,
                         uint64_t triggerUs,
                         uint64_t submissionUs,
                         uint64_t delayUs)
        {
            timingRecords.push_back(
                TimingRecord{triggerUs, submissionUs, delayUs});
        });

    Check(source.AddEvent(event),
          "scenario event is accepted by event source");

    const std::size_t earlySubmissions =
        source.Evaluate(4509999);

    Check(earlySubmissions == 0,
          "scenario event is not submitted before trigger time");

    Check(ns3::g_testSubmittedRequests.empty(),
          "scheduler receives no early scenario request");

    Check(source.PendingEventCount() == 1,
          "scenario event remains pending before trigger time");

    const std::size_t dueSubmissions =
        source.Evaluate(4510000);

    Check(dueSubmissions == 1,
          "scenario event is submitted at trigger time");

    Check(ns3::g_testSubmittedRequests.size() == 1,
          "scheduler receives exactly one scenario request");

    Check(source.PendingEventCount() == 0,
          "scenario event is no longer pending after submission");

    Check(!timingRecords.empty() &&
              timingRecords.back().triggerUs == 4510000 &&
              timingRecords.back().submissionUs == 4510000 &&
              timingRecords.back().delayUs == 0,
          "scenario submission timing is exact");

    const std::size_t repeatedSubmissions =
        source.Evaluate(4520000);

    Check(repeatedSubmissions == 0 &&
              ns3::g_testSubmittedRequests.size() == 1,
          "scenario event is not submitted more than once");
}

void
TestSumoEventRemainsImmediateWithoutDelayUnderflow()
{
    ns3::g_testSubmittedRequests.clear();

    const auto parsed = ns3::SumoDenmEventParser::Parse(
        "DENM_EVENT,veh0,sumo-immediate,DETECTED,"
        "10.5,20.5,94,100,5000,4510000,sumo-origin");

    Check(parsed.has_value(),
          "future-timestamp SUMO event parses");

    if (!parsed)
    {
        return;
    }

    ns3::DenmScenarioEventSource source;

    std::vector<TimingRecord> timingRecords;

    source.SetSubmissionTimingCallback(
        [&timingRecords](const ns3::DenmScenarioEvent&,
                         uint64_t triggerUs,
                         uint64_t submissionUs,
                         uint64_t delayUs)
        {
            timingRecords.push_back(
                TimingRecord{triggerUs, submissionUs, delayUs});
        });

    Check(source.AddEvent(*parsed),
          "SUMO event is accepted by event source");

    const std::size_t submissions =
        source.Evaluate(4400000);

    Check(submissions == 1,
          "SUMO event is submitted immediately");

    Check(ns3::g_testSubmittedRequests.size() == 1 &&
              ns3::g_testSubmittedRequests.front().source ==
                  ns3::DenmTriggerSource::SUMO,
          "immediate request preserves SUMO source");

    Check(!timingRecords.empty() &&
              timingRecords.back().triggerUs == 4510000 &&
              timingRecords.back().submissionUs == 4400000 &&
              timingRecords.back().delayUs == 0,
          "future SUMO timestamp does not cause unsigned delay underflow");
}

void
TestInitialTransmissionDelayUsesSchedulerAcceptanceTime()
{
    ns3::DenmScheduledEvent scheduledEvent;
    scheduledEvent.triggerRequest.triggerTimestampUs = 3000000;
    scheduledEvent.SetInitialTransmissionEligibility(4400000, 50000);

    Check(!scheduledEvent.IsInitialTransmissionDue(4449999),
          "initial DENM transmission is held before acceptance-time delay expires");

    Check(scheduledEvent.IsInitialTransmissionDue(4450000),
          "initial DENM transmission becomes eligible at the exact delay boundary");

    Check(scheduledEvent.triggerRequest.triggerTimestampUs == 3000000,
          "initial transmission delay does not rewrite the DENM trigger timestamp");
}

void
TestZeroInitialTransmissionDelayPreservesImmediateEligibility()
{
    ns3::DenmScheduledEvent scheduledEvent;
    scheduledEvent.SetInitialTransmissionEligibility(4400000, 0);

    Check(scheduledEvent.IsInitialTransmissionDue(4400000),
          "zero initial transmission delay preserves immediate eligibility");
}

} // namespace

int
main()
{
    TestInitialTransmissionDelayUsesSchedulerAcceptanceTime();
    TestZeroInitialTransmissionDelayPreservesImmediateEligibility();
    TestSumoParserPreservesImmediateOrigin();
    TestScenarioParserAcceptsScheduledOrigin();
    TestScenarioEventWaitsUntilTriggerTime();
    TestSumoEventRemainsImmediateWithoutDelayUnderflow();

    std::cout
        << "\nDENM_SCENARIO_SCHEDULING_FAILURES="
        << g_failures
        << '\n';

    return g_failures == 0 ? 0 : 1;
}
