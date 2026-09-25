#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/types/denm_event_id.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/wifi-module.h"

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

int main() {

    std::cout << "===== DENM SCENARIO EVENT SOURCE TEST =====\n";

    ns3::DenmScheduler::Reset();
    ns3::VehicleRegistry::Instance().Clear();

    // O teste inicial usa apenas o registo lógico de um veículo.
    ns3::VehicleRegistry::Instance().CreateOrGet(0, "veh0");

    ns3::DenmScenarioEventSource source;

    ns3::DenmScenarioEvent eventA;
    eventA.vehicleId = "veh0";
    eventA.eventId = "scenario-event-A";
    eventA.triggerTimeUs = 2000000;
    eventA.x = 10.0;
    eventA.y = 20.0;
    eventA.causeCode = 1;
    eventA.relevanceDistanceMeters = 1000.0;
    eventA.validityDurationMs = 5000;
    eventA.reason = "SCENARIO_HAZARD";

    source.AddEvent(eventA);

    bool ok = true;

    source.Evaluate(1999000);

    ok &= Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 0,
        "event is not submitted before trigger time");

    source.Evaluate(2000000);

    ok &= Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "event is submitted at trigger time");

    source.Evaluate(2100000);

    ok &= Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "same scenario event is not submitted twice");

    ns3::DenmScenarioEvent eventB = eventA;
    eventB.eventId = "scenario-event-B";
    eventB.triggerTimeUs = 2200000;

    source.AddEvent(eventB);
    source.Evaluate(2200000);

    ok &= Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 2,
        "independent events from same vehicle coexist");

    ns3::DenmScenarioEvent unknown = eventA;
    unknown.eventId = "unknown-vehicle-event";
    unknown.vehicleId = "veh-does-not-exist";
    unknown.triggerTimeUs = 2300000;

    source.AddEvent(unknown);
    source.Evaluate(2300000);

    ok &= Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 2,
        "unknown vehicle event is not submitted");

    ns3::DenmScheduler::Reset();
    ns3::VehicleRegistry::Instance().Clear();
    ns3::VehicleRegistry::Instance().CreateOrGet(0, "veh0");

    ns3::DenmScenarioEvent sumoEvent = eventA;
    sumoEvent.eventId = "sumo-future-timestamp";
    sumoEvent.triggerTimeUs = 50000000;
    sumoEvent.source = ns3::DenmTriggerSource::SUMO;

    ns3::DenmScenarioEventSource sumoSource;
    sumoSource.AddEvent(sumoEvent);

    sumoSource.Evaluate(3000000);

    ok &= Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "SUMO event is submitted on local ingest despite future SUMO timestamp");

    ns3::DenmTriggerRequest sumoRequest;
    ok &= Expect(
        ns3::DenmScheduler::GetTriggerRequest(
            ns3::DenmEventId{"veh0", "sumo-future-timestamp"},
            sumoRequest),
        "SUMO event remains observable in scheduler");

    ok &= Expect(
        sumoRequest.triggerTimestampUs == 50000000,
        "SUMO source timestamp is preserved");

    bool timingObserved = false;
    std::uint64_t observedTriggerUs = 0;
    std::uint64_t observedSubmissionUs = 0;
    std::uint64_t observedDelayUs = 0;

    ns3::DenmScenarioEvent timingEvent = eventA;
    timingEvent.eventId = "scenario-event-timing";
    timingEvent.triggerTimeUs = 3000000;

    ns3::DenmScenarioEventSource timingSource;
    timingSource.SetSubmissionTimingCallback(
        [&](const ns3::DenmScenarioEvent& event,
            std::uint64_t triggerTimestampUs,
            std::uint64_t submissionTimestampUs,
            std::uint64_t delayUs)
        {
            timingObserved = true;
            observedTriggerUs = triggerTimestampUs;
            observedSubmissionUs = submissionTimestampUs;
            observedDelayUs = delayUs;

            if (event.eventId != "scenario-event-timing")
            {
                timingObserved = false;
            }
        });

    timingSource.AddEvent(timingEvent);
    timingSource.Evaluate(3012345);

    ok &= Expect(
        timingObserved,
        "scenario submission timing is observable");

    ok &= Expect(
        observedTriggerUs == 3000000,
        "timing observation preserves trigger timestamp");

    ok &= Expect(
        observedSubmissionUs == 3012345,
        "timing observation captures evaluation timestamp");

    ok &= Expect(
        observedDelayUs == 12345,
        "timing observation computes trigger-to-submit delay");

    ns3::DenmScheduler::Reset();
    ns3::VehicleRegistry::Instance().Clear();

    // Verify that the source preserves the lifecycle event type through the
    // real NEW -> initial transmission -> UPDATE scheduler lifecycle.
    ns3::YansWifiChannelHelper channelHelper;

    channelHelper.SetPropagationDelay(
        "ns3::ConstantSpeedPropagationDelayModel");

    channelHelper.AddPropagationLoss(
        "ns3::LogDistancePropagationLossModel",
        "Exponent",
        ns3::DoubleValue(2.7),
        "ReferenceDistance",
        ns3::DoubleValue(1.0));

    ns3::Ptr<ns3::YansWifiChannel> channel =
        channelHelper.Create();

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase(
        "10.0.0.0",
        "255.255.255.0");

    ns3::Callback<void, ns3::Ptr<ns3::Socket>> rxCallback;

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        rxCallback);

    ns3::Ptr<ns3::Node> node =
        ns3::VehicleFactory::CreateVehicle("veh0");

    ns3::VehicleState* vehicle =
        ns3::VehicleRegistry::Instance().GetByVehId("veh0");

    ok &= Expect(
        node != nullptr && vehicle != nullptr,
        "vehicle is available for lifecycle scheduler test");

    if (vehicle != nullptr)
    {
        ns3::VehicleState state;
        state.identity.vehId = "veh0";
        state.mobility.position.x = 30.0;
        state.mobility.position.y = 40.0;
        state.mobility.speed = 10.0;
        state.mobility.heading = 0.0;
        state.mobility.accel = 0.0;

        ns3::VehicleStateUpdater::Apply(
            *vehicle,
            state);

        ok &= Expect(
            vehicle->initialized,
            "vehicle is initialized for initial transmission");
    }

    const std::string lifecycleEventId =
        "event-updated";

    ns3::DenmScenarioEvent newEvent;
    newEvent.vehicleId = "veh0";
    newEvent.eventId = lifecycleEventId;
    newEvent.eventType =
        ns3::DenmTriggerEventType::HAZARD_DETECTED;
    newEvent.triggerTimeUs = 3000000;
    newEvent.x = 30.0;
    newEvent.y = 40.0;
    newEvent.causeCode = 1;
    newEvent.relevanceDistanceMeters = 1000.0;
    newEvent.validityDurationMs = 5000;
    newEvent.reason = "hard-brake";

    ns3::DenmScenarioEventSource lifecycleSource;
    lifecycleSource.AddEvent(newEvent);

    ok &= Expect(
        lifecycleSource.Evaluate(3000000) == 1 &&
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "scenario NEW is submitted to scheduler");

    // The scheduler must materialize the initial NEW before UPDATE can be
    // accepted for the same logical event.
    ns3::Simulator::Schedule(
        ns3::Seconds(3.0),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(3.2),
        []()
        {
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();

    ok &= Expect(
        ns3::DenmScheduler::PendingInitialTransmissionCount() == 0,
        "initial NEW transmission completed");

    ns3::DenmScenarioEvent updatedEvent = newEvent;
    updatedEvent.eventType =
        ns3::DenmTriggerEventType::HAZARD_UPDATED;
    updatedEvent.reason = "hard-brake-update";

    lifecycleSource.AddEvent(updatedEvent);

    ok &= Expect(
        lifecycleSource.Evaluate(3200000) == 1,
        "scenario UPDATE is submitted after NEW becomes active");

    ns3::DenmTriggerRequest storedRequest;
    ok &= Expect(
        ns3::DenmScheduler::GetTriggerRequest(
            ns3::DenmEventId{"veh0", lifecycleEventId},
            storedRequest),
        "submitted scenario event is observable in scheduler");

    ok &= Expect(
        storedRequest.eventType ==
            ns3::DenmTriggerEventType::HAZARD_UPDATED,
        "scenario event type reaches scheduler unchanged");

    ns3::Simulator::Destroy();

    ns3::DenmScheduler::Reset();
    ns3::VehicleRegistry::Instance().Clear();

    std::cout << "DENM_SCENARIO_EVENT_SOURCE_EXIT=" << (ok ? 0 : 1)
              << '\n';

    return ok ? 0 : 1;
}
