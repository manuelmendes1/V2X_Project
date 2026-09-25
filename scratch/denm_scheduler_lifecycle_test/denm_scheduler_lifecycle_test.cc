#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/rx/rx_callback/rx_callback.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/wifi-module.h"

#include <iostream>
#include <string>

namespace
{

int failures = 0;

void
Expect(bool condition, const std::string& description)
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
    ns3::DenmTriggerEventType eventType,
    const std::string& eventId,
    const std::string& reason,
    double x)
{
    ns3::DenmTriggerRequest request;

    request.source =
        ns3::DenmTriggerSource::TEST;

    request.eventType =
        eventType;

    request.vehicleId =
        "veh0";

    request.eventId =
        eventId;

    request.x = x;
    request.y = 0.0;

    request.causeCode = 94;

    request.relevanceDistanceMeters =
        1000.0;

    request.validityDurationMs =
        5000;

    request.triggerTimestampUs =
        2000000;

    request.reason =
        reason;

    return request;
}

} // namespace

int
main()
{
    std::cout
        << "===== DENM SCHEDULER LIFECYCLE TEST ====="
        << std::endl;

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();
    ns3::DenmScheduler::Reset();
    ns3::DenmScheduler::SetCompatibilityFallbackEnabled(false);

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

    ns3::Callback<void, ns3::Ptr<ns3::Socket>> rxCallback =
        ns3::MakeCallback(&ns3::RxCallback::Receive);

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        rxCallback);

    ns3::Ptr<ns3::Node> node0 =
        ns3::VehicleFactory::CreateVehicle("veh0");

    ns3::Ptr<ns3::Node> node1 =
        ns3::VehicleFactory::CreateVehicle("veh1");

    ns3::VehicleState* veh0 =
        registry.GetByVehId("veh0");

    ns3::VehicleState* veh1 =
        registry.GetByVehId("veh1");

    Expect(
        node0 != nullptr &&
        node1 != nullptr,
        "two vehicles created");

    Expect(
        veh0 != nullptr &&
        veh1 != nullptr,
        "both vehicles registered");

    ns3::VehicleState state0;
    state0.identity.vehId = "veh0";
    state0.mobility.position.x = 0.0;
    state0.mobility.position.y = 0.0;
    state0.mobility.speed = 10.0;
    state0.mobility.heading = 0.0;
    state0.mobility.accel = 0.0;

    ns3::VehicleState state1;
    state1.identity.vehId = "veh1";
    state1.mobility.position.x = 20.0;
    state1.mobility.position.y = 0.0;
    state1.mobility.speed = 10.0;
    state1.mobility.heading = 0.0;
    state1.mobility.accel = 0.0;

    ns3::VehicleStateUpdater::Apply(
        *veh0,
        state0);

    ns3::VehicleStateUpdater::Apply(
        *veh1,
        state1);

    Expect(
        veh0->initialized &&
        veh1->initialized,
        "both vehicles initialized");

    const std::string eventId =
        "lifecycle-e2e-0001";

    const auto newRequest =
        MakeRequest(
            ns3::DenmTriggerEventType::HAZARD_DETECTED,
            eventId,
            "LIFECYCLE_NEW",
            0.0);

    Expect(
        ns3::DenmScheduler::Submit(newRequest),
        "NEW request accepted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "one logical DENM event scheduled");

    /*
     * First real scheduler execution:
     *   Trigger -> Lifecycle NEW -> Builder -> Encoder -> Sender.
     */
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
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();

    Expect(
        ns3::DenmScheduler::PendingInitialTransmissionCount() == 0,
        "initial NEW transmission completed");

    /*
     * Lifecycle UPDATE:
     *
     * The same logical event must accept an UPDATE after NEW became
     * active. Submission must keep one event and mark a lifecycle
     * transition pending for the scheduler.
     */
    const auto updateRequest =
        MakeRequest(
            ns3::DenmTriggerEventType::HAZARD_UPDATED,
            eventId,
            "LIFECYCLE_UPDATE",
            15.0);

    Expect(
        ns3::DenmScheduler::Submit(updateRequest),
        "UPDATE request for existing event is accepted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "UPDATE keeps one logical event instead of creating a second event");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "UPDATE remains a single active lifecycle event before Run");

    /*
     * Second real scheduler execution:
     *   Trigger -> Lifecycle UPDATE -> Builder -> Encoder -> Sender.
     *
     * The important state assertion is that the pending transition is
     * consumed by Run(). Runtime logs must additionally show:
     *
     *   action=UPDATE action_seq=1
     *
     * proving that UPDATE reuses the ActionID created by NEW.
     */
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
    Expect(
        ns3::DenmScheduler::RepetitionCount(
            ns3::DenmEventId{"veh0", eventId}) == 0,
        "UPDATE transition consumed before repetition scheduling");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "UPDATE leaves the logical event active");

    /*
     * Lifecycle CANCELLATION:
     *
     * The active event must accept a HAZARD_CLEARED request, transmit
     * the cancellation using the same ActionID, and then disappear
     * from the scheduler's active-event store.
     */
    const auto cancellationRequest =
        MakeRequest(
            ns3::DenmTriggerEventType::HAZARD_CLEARED,
            eventId,
            "LIFECYCLE_CANCELLATION",
            15.0);

    Expect(
        ns3::DenmScheduler::Submit(cancellationRequest),
        "CANCELLATION request for existing event is accepted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "CANCELLATION keeps the event until transmission");

    /*
     * Simulator time after the UPDATE run is 5.4 s.
     * This schedule therefore executes at 6.4 s.
     */
    ns3::Simulator::Schedule(
        ns3::Seconds(1.0),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(1.2),
        []()
        {
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 0,
        "CANCELLATION removes the logical event after transmission");

    ns3::Simulator::Destroy();

    ns3::DenmScheduler::Reset();
    registry.Clear();

    std::cout
        << "DENM_SCHEDULER_LIFECYCLE_EXIT="
        << failures
        << std::endl;

    return failures == 0 ? 0 : 1;
}
