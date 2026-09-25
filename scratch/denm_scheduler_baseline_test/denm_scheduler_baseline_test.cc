#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
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

} // namespace

int main(){

    std::cout << "===== DENM SCHEDULER BASELINE TEST ====="
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
        ns3::VehicleFactory::CreateVehicle("veh0");

    ns3::VehicleState* vehicle =
        registry.GetByVehId("veh0");

    Expect(
        node != nullptr,
        "vehicle factory creates veh0");

    Expect(
        vehicle != nullptr,
        "veh0 is present in registry");

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

    // RED: quando o fallback de compatibilidade está desativado,
    // um veículo sem evento explícito não pode gerar baseline-event.
    ns3::DenmScheduler::SetCompatibilityFallbackEnabled(false);

    ns3::Simulator::Schedule(
        ns3::Seconds(2.0),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(2.1),
        []()
        {
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 0,
        "disabled compatibility fallback does not create baseline-event");

    ns3::DenmScheduler::Reset();
    ns3::DenmScheduler::SetCompatibilityFallbackEnabled(true);

    const ns3::DenmEventId baselineKey{
        "veh0",
        "baseline-event"};

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
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "baseline-event remains scheduled");

    Expect(
        ns3::DenmScheduler::PendingInitialTransmissionCount() == 0,
        "baseline initial transmission is not left pending");

    Expect(
        ns3::DenmScheduler::RepetitionCount(baselineKey) == 3,
        "baseline-event reaches exactly three repetitions");

    ns3::DenmScheduler::Reset();
    registry.Clear();

    std::cout
        << "DENM_SCHEDULER_BASELINE_EXIT="
        << failures
        << std::endl;

    return failures == 0 ? 0 : 1;
}
