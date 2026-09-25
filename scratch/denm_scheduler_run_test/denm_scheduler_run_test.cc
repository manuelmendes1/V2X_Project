#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/denm/types/denm_event_id.h"


#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/wifi-module.h"


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
        2000000;

    request.reason =
        "RUN_MULTI_EVENT_" + eventId;

    return request;
}

} // namespace

int main()
{
    std::cout << "===== DENM SCHEDULER RUN TEST ====="
              << std::endl;

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();
    ns3::DenmScheduler::Reset();

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
        VehicleFactory::CreateVehicle("veh0");

    ns3::VehicleState* vehicle =
        registry.GetByVehId("veh0");

    Expect(
        node != nullptr,
        "vehicle factory creates veh0");

    Expect(
        vehicle != nullptr,
        "veh0 is present in registry");

    Expect(
        vehicle != nullptr &&
        vehicle->socket != nullptr,
        "veh0 has a UDP socket");

    Expect(
        vehicle != nullptr &&
        vehicle->node != nullptr,
        "veh0 has an ns-3 node");

    Expect(
        vehicle != nullptr &&
        vehicle->identity.stationId != 0,
        "veh0 has a valid stationId");

    // Reproduzir a atualização de estado proveniente do SUMO.
    ns3::VehicleState parsedState;
    parsedState.identity.vehId = "veh0";
    parsedState.mobility.position.x = 0.0;
    parsedState.mobility.position.y = 0.0;
    parsedState.mobility.speed = 10.0;
    parsedState.mobility.heading = 0.0;
    parsedState.mobility.accel = 0.0;

    Expect(
        vehicle != nullptr,
        "veh0 exists before SUMO state update");

    ns3::VehicleStateUpdater::Apply(
        *vehicle,
        parsedState);

    Expect(
        vehicle != nullptr &&
        vehicle->initialized,
        "veh0 is initialized");

    const auto eventA =
        MakeRequest("event-A");

    const auto eventB =
        MakeRequest("event-B");

    Expect(
        ns3::DenmScheduler::Submit(eventA),
        "event-A submitted");

    Expect(
        ns3::DenmScheduler::Submit(eventB),
        "event-B submitted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 2,
        "two events are scheduled before Run");

    Expect(
        ns3::DenmScheduler::PendingInitialTransmissionCount() == 2,
        "both submitted events are pending initial transmission");

        const ns3::DenmEventId eventKeyA{
        "veh0",
        "event-A"};

    const ns3::DenmEventId eventKeyB{
        "veh0",
        "event-B"};

    ns3::Simulator::Schedule(
        ns3::Seconds(2.0),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(2.2),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(2.4),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(2.6),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(2.8),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(2.9),
        []()
        {
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    Expect(
        ns3::DenmScheduler::PendingInitialTransmissionCount() == 0,
        "all initial NEW transmissions completed");

    Expect(
        ns3::DenmScheduler::RepetitionCount(eventKeyA) == 3,
        "event-A reaches exactly three repetitions");

    Expect(
        ns3::DenmScheduler::RepetitionCount(eventKeyB) == 3,
        "event-B reaches exactly three repetitions");

    Expect(
        ns3::DenmScheduler::RepetitionCount(eventKeyA) ==
        ns3::DenmScheduler::RepetitionCount(eventKeyB),
        "event-A and event-B repetition counts remain independent and equal");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 2,
        "both events remain scheduled after maximum repetition count");

    ns3::DenmScheduler::Reset();
    registry.Clear();

    std::cout
        << "DENM_SCHEDULER_RUN_EXIT="
        << failures
        << std::endl;

    return failures == 0 ? 0 : 1;

}
