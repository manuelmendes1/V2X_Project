#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"

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
    const std::string& eventId,
    uint64_t timestampUs,
    const std::string& reason)
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

    request.x = 100.0;
    request.y = 200.0;

    request.causeCode = 94;

    request.relevanceDistanceMeters =
        100.0;

    request.validityDurationMs =
        5000;

    request.triggerTimestampUs =
        timestampUs;

    request.reason =
        reason;

    return request;
}

} // namespace

int
main(int argc, char* argv[])
{
    std::cout
        << "===== DENM V3 SCHEDULER E2E CREDENTIAL ROTATION TEST ====="
        << std::endl;

    std::string itsStartTime;

    ns3::CommandLine cmd;

    cmd.AddValue(
        "itsStartTime",
        "Explicit ITS start time for V3 certificate validation",
        itsStartTime);

    cmd.Parse(argc, argv);

    if (itsStartTime.empty())
    {
        std::cerr
            << "FAIL --itsStartTime is required for DENM V3 scheduler E2E test"
            << std::endl;
        return 1;
    }

    SimulationContext::itsStartTime = itsStartTime;

    std::cout
        << "ITS_START_TIME="
        << SimulationContext::itsStartTime
        << std::endl;

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();
    ns3::DenmScheduler::Reset();
    ns3::DenmScheduler::SetCompatibilityFallbackEnabled(false);

    const char* home =
        std::getenv("HOME");

    const std::string pkiDirectory =
        std::string(home != nullptr ? home : ".") +
        "/vanetza-pki-lab/validation-denm-pki";

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        pkiDirectory);

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
        "real VehicleFactory creates veh0");

    Expect(
        vehicle != nullptr,
        "veh0 is registered");

    Expect(
        vehicle != nullptr &&
        vehicle->runtime != nullptr,
        "veh0 has a real VanetzaRuntime");

    // Reproduzir a condição de veículo "ready" exigida por
    // VehicleDispatcher::ForEachReadyVehicle(), tal como no
    // teste denm_scheduler_run_test existente.
    ns3::VehicleState parsedState;
    parsedState.identity.vehId = "veh0";
    parsedState.mobility.position.x = 100.0;
    parsedState.mobility.position.y = 200.0;
    parsedState.mobility.speed = 10.0;
    parsedState.mobility.heading = 0.0;
    parsedState.mobility.accel = 0.0;

    Expect(
        vehicle != nullptr,
        "veh0 exists before scheduler state update");

    if (vehicle != nullptr)
    {
        ns3::VehicleStateUpdater::Apply(
            *vehicle,
            parsedState);
    }

    Expect(
        vehicle != nullptr &&
        vehicle->initialized,
        "veh0 is ready before DenmScheduler::Run");

    Expect(
        vehicle != nullptr &&
        vehicle->security
            .credentialPool.Size() >= 2,
        "veh0 has at least two DENM credentials");

    Expect(
        vehicle != nullptr &&
        vehicle->security
            .credentialPool.GetActiveCredentialIndex() == 0,
        "credential 000 is active before scheduler transmission");

    std::vector<std::shared_ptr<const ns3::PacketContext>>
        transmissions;

    ns3::DenmScheduler::SetTransmissionContextObserver(
        [&transmissions](
            std::shared_ptr<const ns3::PacketContext> context)
        {
            transmissions.push_back(
                std::move(context));
        });

    const auto firstRequest =
        MakeRequest(
            "scheduler-event-A",
            1000000,
            "SCHEDULER_E2E_FIRST");

    const auto secondRequest =
        MakeRequest(
            "scheduler-event-B",
            1100000,
            "SCHEDULER_E2E_SECOND");

    Expect(
        ns3::DenmScheduler::Submit(firstRequest),
        "first DENM is accepted by DenmScheduler::Submit");

    Expect(
        ns3::DenmScheduler::ScheduledEventCount() == 1,
        "one event is scheduled after first Submit");

    Expect(
        ns3::DenmScheduler::PendingInitialTransmissionCount() == 1,
        "first event is pending initial transmission");

    std::shared_ptr<const ns3::PacketContext> firstContext;
    std::shared_ptr<const ns3::PacketContext> secondContext;
    std::string firstSignerCertificateIdAtSign;

    ns3::Simulator::Schedule(
        ns3::Seconds(1.0),
        [&]()
        {
            ns3::DenmScheduler::Run(100.0);

            Expect(
                transmissions.size() == 1,
                "DenmScheduler::Run materializes exactly the first DENM");

            if (!transmissions.empty())
            {
                firstContext = transmissions.back();

                Expect(
                    firstContext != nullptr,
                    "scheduler observer receives first PacketContext");

                Expect(
                    firstContext != nullptr &&
                    firstContext->signerCertificateIdHex.size() == 16,
                    "first scheduler DENM exposes a V3 HashedId8");

                if (firstContext)
                {
                    firstSignerCertificateIdAtSign =
                        firstContext->signerCertificateIdHex;

                    Expect(
                        !firstSignerCertificateIdAtSign.empty(),
                        "first scheduler DENM captures signer identity after SIGN");
                }

                Expect(
                    firstContext != nullptr &&
                    firstContext->pipelineStage !=
                        ns3::PipelineStage::NONE,
                    "first scheduler DENM has an active pipeline stage");
            }

            Expect(
                ns3::DenmScheduler::PendingInitialTransmissionCount() == 0,
                "first scheduler DENM leaves initial transmission pending count at zero");
        });

    ns3::Simulator::Schedule(
        ns3::MilliSeconds(1050),
        [&]()
        {
            Expect(
                vehicle != nullptr &&
                vehicle->runtime->RotateDenmCredential(),
                "scheduler test rotates DENM credential through real runtime");

            Expect(
                vehicle != nullptr &&
                vehicle->security
                    .credentialPool.GetActiveCredentialIndex() == 1,
                "credential 001 is active before second scheduler DENM");

            Expect(
                vehicle != nullptr &&
                vehicle->security
                    .certFile.find("veh0-ticket-001.cert") !=
                    std::string::npos,
                "credential 001 certificate is active before second scheduler DENM");
        });

    ns3::Simulator::Schedule(
        ns3::MilliSeconds(1100),
        [&]()
        {
            Expect(
                ns3::DenmScheduler::Submit(secondRequest),
                "second DENM is accepted by DenmScheduler::Submit");

            Expect(
                ns3::DenmScheduler::ScheduledEventCount() == 2,
                "two independent events are present before second Run");

            ns3::DenmScheduler::Run(100.0);

            Expect(
                transmissions.size() == 2,
                "second DenmScheduler::Run materializes exactly one new DENM");

            if (transmissions.size() >= 2)
            {
                secondContext = transmissions.back();

                Expect(
                    secondContext != nullptr,
                    "scheduler observer receives second PacketContext");

                Expect(
                    secondContext != nullptr &&
                    secondContext->signerCertificateIdHex.size() == 16,
                    "second scheduler DENM exposes a V3 HashedId8");
            }
        });

    ns3::Simulator::Schedule(
        ns3::MilliSeconds(1300),
        [&]()
        {
            Expect(
                firstContext != nullptr &&
                firstContext->pipelineStage ==
                    ns3::PipelineStage::TX_COMPLETE,
                "first scheduler DENM reaches TX_COMPLETE");

            Expect(
                firstContext != nullptr &&
                firstContext->pipelineResult ==
                    ns3::PipelineResult::OK,
                "first scheduler DENM TX_COMPLETE result is OK");

            Expect(
                secondContext != nullptr &&
                secondContext->pipelineStage ==
                    ns3::PipelineStage::TX_COMPLETE,
                "second scheduler DENM reaches TX_COMPLETE");

            Expect(
                secondContext != nullptr &&
                secondContext->pipelineResult ==
                    ns3::PipelineResult::OK,
                "second scheduler DENM TX_COMPLETE result is OK");

            Expect(
                firstContext != nullptr &&
                secondContext != nullptr &&
                !firstContext->signerCertificateIdHex.empty() &&
                !secondContext->signerCertificateIdHex.empty(),
                "both scheduler DENMs expose signer identity");

            Expect(
                firstContext != nullptr &&
                !firstSignerCertificateIdAtSign.empty() &&
                firstContext->signerCertificateIdHex ==
                    firstSignerCertificateIdAtSign,
                "PKT=1 signer remains unchanged until TX_COMPLETE");

            Expect(
                firstContext != nullptr &&
                secondContext != nullptr &&
                firstContext->signerCertificateIdHex !=
                    secondContext->signerCertificateIdHex,
                "scheduler DENM signer identity changes after credential rotation");

            if (firstContext && secondContext)
            {
                std::cout
                    << "FIRST_SCHEDULER_SIGNER_CERT_ID="
                    << firstContext->signerCertificateIdHex
                    << std::endl;

                std::cout
                    << "SECOND_SCHEDULER_SIGNER_CERT_ID="
                    << secondContext->signerCertificateIdHex
                    << std::endl;
            }

            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    ns3::DenmScheduler::Reset();
    registry.Clear();

    std::cout
        << "DENM_V3_SCHEDULER_E2E_CREDENTIAL_ROTATION_EXIT="
        << failures
        << std::endl;

    return failures == 0 ? 0 : 1;
}
