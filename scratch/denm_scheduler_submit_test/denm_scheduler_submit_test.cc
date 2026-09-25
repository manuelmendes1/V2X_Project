#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <iostream>
#include <string>

namespace
{

int failures = 0;

void
Expect(
    bool condition,
    const std::string& description)
{
    if (condition)
    {
        std::cout << "PASS " << description << std::endl;
    }
    else
    {
        std::cout << "FAIL " << description << std::endl;
        ++failures;
    }
}

ns3::DenmTriggerRequest
MakeRequest(
    const std::string& eventId)
{
    ns3::DenmTriggerRequest request;

    request.source =
        ns3::DenmTriggerSource::TEST;

    request.eventType =
        ns3::DenmTriggerEventType::HAZARD_DETECTED;

    request.vehicleId =
        "veh0";

    request.eventId =
        eventId;

    request.x = 10.0;
    request.y = 20.0;

    request.causeCode = 94;

    request.relevanceDistanceMeters =
        1000.0;

    request.validityDurationMs =
        5000;

    request.triggerTimestampUs =
        1000000;

    request.reason =
        "MULTI_EVENT_TEST";

    return request;
}

} // namespace

int main()
{
    std::cout << "===== DENM SCHEDULER SUBMIT TEST ====="
              << std::endl;

    ns3::DenmScheduler::Reset();

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();

    registry.CreateOrGet(
        0,
        "veh0");

    const auto eventA =
        MakeRequest("event-A");

    const auto eventB =
        MakeRequest("event-B");

    Expect(
        ns3::DenmScheduler::Submit(eventA),
        "event-A submission is accepted");

    Expect(
        ns3::DenmScheduler::Submit(eventB),
        "event-B submission from same vehicle is accepted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 2,
        "two events from same vehicle coexist in scheduler");

    ns3::DenmTriggerRequest invalid =
        MakeRequest("");

    Expect(
        !ns3::DenmScheduler::Submit(invalid),
        "invalid event identity is rejected");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 2,
        "invalid submission does not alter scheduler state");

    ns3::DenmTriggerRequest unknownVehicle =
        MakeRequest("event-unknown-vehicle");

    unknownVehicle.vehicleId =
        "veh-not-registered";

    Expect(
        !ns3::DenmScheduler::Submit(unknownVehicle),
        "event for unknown vehicle is rejected");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 2,
        "unknown vehicle submission does not alter scheduler state");

    ns3::DenmScheduler::Reset();

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 0,
        "scheduler reset removes all scheduled events");

    registry.Clear();

    std::cout
        << "DENM_SCHEDULER_SUBMIT_EXIT="
        << failures
        << std::endl;

    return failures == 0 ? 0 : 1;
}
