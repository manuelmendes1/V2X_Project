#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"

#include "vanetza_integration/denm/builder/denm_builder.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/types/denm_input.h"
#include "vanetza_integration/denm/sender/denm_sender.h"
#include "vanetza_integration/denm/service/denm_service.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"

#include "ns3/callback.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-module.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include "vanetza_integration/tests/denm_test_pki.h"

namespace
{

bool
Check(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << "\n";
        return false;
    }

    std::cout << "PASS " << message << "\n";
    return true;
}

ns3::DenmTriggerRequest
MakeRequest(
    const std::string& eventId,
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

    request.x =
        100.0;

    request.y =
        200.0;

    request.causeCode =
        94;

    request.relevanceDistanceMeters =
        100.0;

    request.validityDurationMs =
        5000;

    request.triggerTimestampUs =
        1000000;

    request.reason =
        reason;

    return request;
}

bool
GenerateAndSend(
    ns3::VehicleState& vehicle,
    ns3::DenmLifecycleManager& lifecycle,
    const ns3::DenmTriggerRequest& request,
    std::shared_ptr<ns3::PacketContext>& outContext,
    ns3::DenmLifecycleResult& outLifecycleResult)
{
    ns3::DenmLifecycleResult lifecycleResult;

    bool lifecycleAccepted = false;

    ns3::DenmTrigger trigger(
        [&](const ns3::DenmTriggerRequest& acceptedRequest)
        {
            lifecycleAccepted =
                lifecycle.Process(
                    vehicle,
                    acceptedRequest,
                    lifecycleResult);
        });

    if (!trigger.Submit(request) ||
        !lifecycleAccepted)
    {
        return false;
    }

    outLifecycleResult =
        lifecycleResult;

    auto generation =
        ns3::DenmBuilder::Build(
            vehicle,
            request,
            lifecycleResult);

    if (!generation.packetContext ||
        generation.denmInput.actionId.sequenceNumber == 0)
    {
        lifecycle.Abort(
            request.vehicleId,
            request.eventId);

        return false;
    }

    const auto rawPayload =
        ns3::DenmService::GenerateRawPayload(
            generation,
            request.reason);

    if (rawPayload.empty())
    {
        lifecycle.Abort(
            request.vehicleId,
            request.eventId);

        return false;
    }

    outContext =
        generation.packetContext;

    if (!ns3::DenmSender::SendRaw(
            outContext,
            vehicle,
            generation.denmInput,
            rawPayload,
            request.relevanceDistanceMeters,
            request.reason))
    {
        lifecycle.Abort(
            request.vehicleId,
            request.eventId);

        return false;
    }

    return true;
}

} // namespace

int
main()
{
    std::cout
        << "===== DENM V3 RUNTIME E2E CREDENTIAL ROTATION TEST =====\n";

    const std::string pkiDir =
        ns3::denm_test::ResolveDenmPkiDirectory();

    if (pkiDir.empty())
    {
        std::cerr
            << "FAIL DENM test PKI directory is unavailable; "
               "set DENM_TEST_PKI_DIR or provide "
               "$HOME/vanetza-pki-lab/validation-denm-pki\n";
        return 1;
    }

    bool ok = true;

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        pkiDir);

    ns3::DenmLifecycleManager lifecycle;

    lifecycle.Reset();

    SimulationContext::itsStartTime =
        "2026-09-24T13:38:21";

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();

    auto channel =
        ns3::YansWifiChannelHelper::Default().Create();

    ns3::Ipv4AddressHelper ipv4;

    ipv4.SetBase(
        "10.0.0.0",
        "255.255.255.0");

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        ns3::MakeNullCallback<
            void,
            ns3::Ptr<ns3::Socket>>());

    ns3::VehicleFactory::CreateVehicle(
        "veh0");

    ns3::VehicleState* vehicle =
        registry.GetByVehId("veh0");

    ok &= Check(
        vehicle != nullptr,
        "veh0 is created through the real VehicleFactory");

    if (vehicle == nullptr)
    {
        return 1;
    }

    ok &= Check(
        vehicle->runtime != nullptr &&
            vehicle->runtime->IsInitialized(),
        "VanetzaRuntime is initialized");

    ok &= Check(
        vehicle->security.denmCredentialPool.Size() >= 2,
        "veh0 has at least two DENM credentials");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredentialIndex() == 0,
        "credential 000 is active before first DENM");

    std::shared_ptr<ns3::PacketContext> firstContext;
    std::shared_ptr<ns3::PacketContext> secondContext;

    ns3::DenmLifecycleResult firstLifecycleResult{};
    ns3::DenmLifecycleResult secondLifecycleResult{};

    ns3::DenmTriggerRequest firstRequest =
        MakeRequest(
            "e2e-rotation-000",
            "E2E_ROTATION_FIRST");

    ns3::DenmTriggerRequest secondRequest =
        firstRequest;

    secondRequest.eventType =
        ns3::DenmTriggerEventType::HAZARD_UPDATED;

    secondRequest.triggerTimestampUs =
        1200000;

    secondRequest.reason =
        "E2E_ROTATION_SECOND";

    ns3::Simulator::Schedule(
        ns3::Seconds(1.0),
        [&]()
        {
            ok &= Check(
                GenerateAndSend(
                    *vehicle,
                    lifecycle,
                    firstRequest,
                    firstContext,
                    firstLifecycleResult),
                "first DENM traverses trigger -> lifecycle -> builder -> service -> sender");

            ok &= Check(
                firstContext != nullptr,
                "first DENM retains PacketContext until asynchronous TX completes");

            if (firstContext)
            {
                ok &= Check(
                    !firstContext->signerCertificateIdHex.empty(),
                    "first DENM exposes the real V3 signer certificate identity");
            }
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(1.1),
        [&]()
        {
            if (!vehicle->runtime)
            {
                ok = false;
                return;
            }

            ok &= Check(
                vehicle->runtime->RotateDenmCredential(),
                "runtime credential rotation succeeds between DENM transmissions");

            ok &= Check(
                vehicle->security.denmCredentialPool.GetActiveCredentialIndex() == 1,
                "credential 001 is active before second DENM");

            ok &= Check(
                vehicle->security.certFile.ends_with(
                    "veh0-ticket-001.cert"),
                "credential 001 certificate is active before second DENM");
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(1.2),
        [&]()
        {
            ok &= Check(
                GenerateAndSend(
                    *vehicle,
                    lifecycle,
                    secondRequest,
                    secondContext,
                    secondLifecycleResult),
                "second DENM traverses trigger -> lifecycle -> builder -> service -> sender");

            ok &= Check(
                secondContext != nullptr,
                "second DENM retains PacketContext until asynchronous TX completes");

            if (secondContext)
            {
                ok &= Check(
                    !secondContext->signerCertificateIdHex.empty(),
                    "second DENM exposes the real V3 signer certificate identity");
            }

            ok &= Check(
                secondLifecycleResult.action ==
                    DenmManagementAction::UPDATE,
                "second DENM is an UPDATE of the existing lifecycle event");

            ok &= Check(
                firstLifecycleResult.actionId.originatingStationId ==
                    secondLifecycleResult.actionId.originatingStationId &&
                    firstLifecycleResult.actionId.sequenceNumber ==
                    secondLifecycleResult.actionId.sequenceNumber,
                "DENM UPDATE preserves the same ActionID");

            ok &= Check(
                firstContext != nullptr &&
                    secondContext != nullptr &&
                    firstContext->senderStationId ==
                    secondContext->senderStationId,
                "DENM lifecycle preserves stationID across credential rotation");
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(1.4),
        [&]()
        {
            if (!firstContext ||
                !secondContext)
            {
                ok = false;
                return;
            }

            ok &= Check(
                firstContext->pipelineStage ==
                    ns3::PipelineStage::TX_COMPLETE,
                "first DENM reaches TX_COMPLETE");

            ok &= Check(
                firstContext->pipelineResult ==
                    ns3::PipelineResult::OK,
                "first DENM TX_COMPLETE result is OK");

            ok &= Check(
                secondContext->pipelineStage ==
                    ns3::PipelineStage::TX_COMPLETE,
                "second DENM reaches TX_COMPLETE");

            ok &= Check(
                secondContext->pipelineResult ==
                    ns3::PipelineResult::OK,
                "second DENM TX_COMPLETE result is OK");

            ok &= Check(
                firstContext->signerCertificateIdHex !=
                    secondContext->signerCertificateIdHex,
                "DENM signer identity changes after runtime credential rotation");

            std::cout
                << "FIRST_SIGNER_CERT_ID="
                << firstContext->signerCertificateIdHex
                << "\n";

            std::cout
                << "SECOND_SIGNER_CERT_ID="
                << secondContext->signerCertificateIdHex
                << "\n";
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(1.6),
        [&]()
        {
            ns3::Simulator::Stop();
        });

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    lifecycle.Reset();

    registry.Clear();

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory("");

    std::cout
        << "DENM_V3_RUNTIME_E2E_CREDENTIAL_ROTATION_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
