#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"
#include "vanetza_integration/denm/scenario/sumo_denm_event_parser.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/types/denm_event_id.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace
{

bool
Expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }

    std::cout << "PASS " << message << '\n';
    return true;
}

} // namespace

int main()
{
    std::cout << "===== DENM SUMO CLOCK DOMAIN TEST =====\n";

    bool ok = true;

    ns3::DenmScheduler::Reset();
    ns3::VehicleRegistry::Instance().Clear();

    ns3::VehicleRegistry::Instance().CreateOrGet(0, "veh0");

    const std::string message =
        "DENM_EVENT,veh0,future-sumo-event,DETECTED,"
        "10.0,20.0,3,1000.0,5000,50000000,traffic_jam";

    const auto parsed =
        ns3::SumoDenmEventParser::Parse(message);

    ok &= Expect(
        parsed.has_value(),
        "SUMO DENM wire event is parsed");

    if (!parsed)
    {
        std::cout << "DENM_SUMO_CLOCK_DOMAIN_EXIT="
                  << (ok ? 0 : 1)
                  << '\n';
        return ok ? 0 : 1;
    }

    ok &= Expect(
        parsed->source == ns3::DenmTriggerSource::SUMO,
        "parsed event belongs to SUMO time domain");

    ok &= Expect(
        parsed->triggerTimeUs == 50000000,
        "SUMO trigger timestamp is preserved");

    bool timingObserved = false;
    std::uint64_t observedTriggerUs = 0;
    std::uint64_t observedSubmitUs = 0;

    ns3::DenmScenarioEventSource source;

    source.SetSubmissionTimingCallback(
        [&](const ns3::DenmScenarioEvent& event,
            std::uint64_t triggerTimestampUs,
            std::uint64_t submissionTimestampUs,
            std::uint64_t)
        {
            timingObserved =
                event.eventId == "future-sumo-event";

            observedTriggerUs = triggerTimestampUs;
            observedSubmitUs = submissionTimestampUs;
        });

    ok &= Expect(
        source.AddEvent(*parsed),
        "parsed SUMO event is accepted by scenario source");

    // Deliberately earlier than the SUMO timestamp:
    // 3 s ns-3 versus 50 s SUMO.
    source.Evaluate(3000000);

    ok &= Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "SUMO event is submitted without waiting for SUMO timestamp");

    ns3::DenmTriggerRequest storedRequest;

    ok &= Expect(
        ns3::DenmScheduler::GetTriggerRequest(
            ns3::DenmEventId{"veh0", "future-sumo-event"},
            storedRequest),
        "SUMO event is observable in scheduler");

    ok &= Expect(
        storedRequest.source == ns3::DenmTriggerSource::SUMO,
        "scheduler preserves SUMO source");

    ok &= Expect(
        storedRequest.triggerTimestampUs == 50000000,
        "scheduler preserves original SUMO timestamp");

    ok &= Expect(
        timingObserved,
        "SUMO submission timing is observable");

    ok &= Expect(
        observedTriggerUs == 50000000,
        "timing callback preserves SUMO trigger timestamp");

    ok &= Expect(
        observedSubmitUs == 3000000,
        "timing callback records local ns-3 evaluation time");

    ns3::DenmScheduler::Reset();
    ns3::VehicleRegistry::Instance().Clear();

    std::cout << "DENM_SUMO_CLOCK_DOMAIN_EXIT="
              << (ok ? 0 : 1)
              << '\n';

    return ok ? 0 : 1;
}
