#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.h"

#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/rx/rx_callback/rx_callback.h"
#include "vanetza_integration/context/packet/packet_context.h"

#include "ns3/command-line.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/wifi-module.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

void
RunSchedulerWindow(double runAfterSeconds)
{
    ns3::Simulator::Schedule(
        ns3::Seconds(runAfterSeconds),
        []()
        {
            ns3::DenmScheduler::Run(1000.0);
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(runAfterSeconds + 0.2),
        []()
        {
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();
}

} // namespace

int main(int argc, char* argv[]) {

    std::cout
        << "============================================================"
        << std::endl;
    std::cout
        << "P21.1 — DENM V3 LIFECYCLE E2E TEST"
        << std::endl;
    std::cout
        << "============================================================"
        << std::endl;

    std::string itsStartTime;

    const char* home =
        std::getenv("HOME");

    std::string pkiDirectory =
        std::string(home != nullptr ? home : ".") +
        "/vanetza-pki-lab/validation-denm-pki";

    ns3::CommandLine cmd;

    cmd.AddValue(
        "itsStartTime",
        "Explicit ITS start time for V3 certificate validation",
        itsStartTime);

    cmd.AddValue(
        "denmPkiDir",
        "DENM V3 PKI directory",
        pkiDirectory);

    cmd.Parse(argc, argv);

    if (itsStartTime.empty())
    {
        std::cerr
            << "FAIL --itsStartTime is required"
            << std::endl;
        return 1;
    }

    SimulationContext::itsStartTime =
        itsStartTime;

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        pkiDirectory);

    std::cout
        << "ITS_START_TIME="
        << itsStartTime
        << std::endl;

    std::cout
        << "DENM_PKI_DIR="
        << pkiDirectory
        << std::endl;

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();

    ns3::DenmScheduler::Reset();

    ns3::DenmScheduler::SetCompatibilityFallbackEnabled(
        false);

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
        ns3::MakeCallback(
            &ns3::RxCallback::Receive);

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        rxCallback);

    ns3::Ptr<ns3::Node> node0 =
        ns3::VehicleFactory::CreateVehicle(
            "veh0");

    ns3::Ptr<ns3::Node> node1 =
        ns3::VehicleFactory::CreateVehicle(
            "veh1");

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

    if (veh0 == nullptr || veh1 == nullptr)
    {
        return 1;
    }

    Expect(
        veh0->security.credentialPool.Size() >= 1,
        "veh0 has a real DENM credential");

    Expect(
        veh1->security.credentialPool.Size() >= 1,
        "veh1 has a real DENM credential");

    Expect(
        !veh0->security.certFile.empty() &&
        !veh0->security.keyFile.empty(),
        "veh0 has real certificate/key paths");

    Expect(
        !veh1->security.certFile.empty() &&
        !veh1->security.keyFile.empty(),
        "veh1 has real certificate/key paths");

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

    Expect(
        veh0->identity.stationId != 0 &&
        veh1->identity.stationId != 0 &&
        veh0->identity.stationId != veh1->identity.stationId,
        "vehicles have distinct non-zero station IDs");

    std::vector<std::shared_ptr<const ns3::PacketContext>>
        transmissions;

    ns3::DenmScheduler::SetTransmissionContextObserver(
        [&transmissions](
            std::shared_ptr<const ns3::PacketContext> context)
        {
            transmissions.push_back(
                std::move(context));
        });

    const std::string eventId =
        "p21-1-lifecycle-0001";

    // ---------------------------------------------------------------------
    // NEW
    // ---------------------------------------------------------------------

    const auto newRequest =
        MakeRequest(
            ns3::DenmTriggerEventType::HAZARD_DETECTED,
            eventId,
            "P21_1_NEW",
            0.0);

    Expect(
        ns3::DenmScheduler::Submit(newRequest),
        "NEW request accepted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "one logical event scheduled after NEW");

    RunSchedulerWindow(2.0);

    Expect(
        transmissions.size() == 1,
        "NEW produced exactly one observed physical transmission");

    if (transmissions.size() >= 1)
    {
        Expect(
            transmissions[0] != nullptr,
            "NEW PacketContext exists");

        Expect(
            transmissions[0] != nullptr &&
            transmissions[0]->packetId != 0,
            "NEW PacketContext has physical packet ID");

        Expect(
            transmissions[0] != nullptr &&
            transmissions[0]->pipelineStage ==
                ns3::PipelineStage::TX_COMPLETE,
            "NEW reaches TX_COMPLETE");

        Expect(
            transmissions[0] != nullptr &&
            transmissions[0]->pipelineResult ==
                ns3::PipelineResult::OK,
            "NEW TX_COMPLETE result is OK");
    }

    // ---------------------------------------------------------------------
    // UPDATE
    // ---------------------------------------------------------------------

    const auto updateRequest =
        MakeRequest(
            ns3::DenmTriggerEventType::HAZARD_UPDATED,
            eventId,
            "P21_1_UPDATE",
            15.0);

    Expect(
        ns3::DenmScheduler::Submit(updateRequest),
        "UPDATE request accepted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "UPDATE preserves one logical event");

    RunSchedulerWindow(1.0);

    Expect(
        transmissions.size() == 2,
        "UPDATE produced exactly one additional physical transmission");

    if (transmissions.size() >= 2)
    {
        Expect(
            transmissions[1] != nullptr &&
            transmissions[1]->packetId != 0,
            "UPDATE PacketContext has physical packet ID");

        Expect(
            transmissions[1] != nullptr &&
            transmissions[1]->pipelineStage ==
                ns3::PipelineStage::TX_COMPLETE,
            "UPDATE reaches TX_COMPLETE");

        Expect(
            transmissions[1] != nullptr &&
            transmissions[1]->pipelineResult ==
                ns3::PipelineResult::OK,
            "UPDATE TX_COMPLETE result is OK");

        Expect(
            transmissions[0] != nullptr &&
            transmissions[1] != nullptr &&
            transmissions[0]->packetId !=
                transmissions[1]->packetId,
            "NEW and UPDATE use distinct physical packet IDs");
    }

    // ---------------------------------------------------------------------
    // CANCELLATION
    // ---------------------------------------------------------------------

    const auto cancellationRequest =
        MakeRequest(
            ns3::DenmTriggerEventType::HAZARD_CLEARED,
            eventId,
            "P21_1_CANCELLATION",
            15.0);

    Expect(
        ns3::DenmScheduler::Submit(cancellationRequest),
        "CANCELLATION request accepted");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "CANCELLATION keeps the logical event until transmission");

    RunSchedulerWindow(1.0);

    Expect(
        transmissions.size() == 3,
        "CANCELLATION produced exactly one additional physical transmission");

    if (transmissions.size() >= 3)
    {
        Expect(
            transmissions[2] != nullptr &&
            transmissions[2]->packetId != 0,
            "CANCELLATION PacketContext has physical packet ID");

        Expect(
            transmissions[2] != nullptr &&
            transmissions[2]->pipelineStage ==
                ns3::PipelineStage::TX_COMPLETE,
            "CANCELLATION reaches TX_COMPLETE");

        Expect(
            transmissions[2] != nullptr &&
            transmissions[2]->pipelineResult ==
                ns3::PipelineResult::OK,
            "CANCELLATION TX_COMPLETE result is OK");

        Expect(
            transmissions[1] != nullptr &&
            transmissions[2] != nullptr &&
            transmissions[1]->packetId !=
                transmissions[2]->packetId,
            "UPDATE and CANCELLATION use distinct physical packet IDs");
    }

    Expect(
        transmissions.size() >= 3 &&
        transmissions[0] != nullptr &&
        transmissions[1] != nullptr &&
        transmissions[2] != nullptr &&
        transmissions[0]->packetId != transmissions[2]->packetId,
        "all three lifecycle transmissions use distinct physical packet IDs");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 0,
        "CANCELLATION terminates the active logical event");

    ns3::Simulator::Destroy();

    ns3::DenmScheduler::SetTransmissionContextObserver(
        {});

    ns3::DenmScheduler::Reset();

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        "");

    SimulationContext::itsStartTime.clear();

    registry.Clear();

    std::cout
        << "DENM_V3_LIFECYCLE_E2E_EXIT="
        << (failures == 0 ? 0 : 1)
        << std::endl;

    return failures == 0 ? 0 : 1;
}
