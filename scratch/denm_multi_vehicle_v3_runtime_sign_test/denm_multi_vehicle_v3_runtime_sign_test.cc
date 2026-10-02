#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"

#include "vanetza_integration/denm/builder/denm_builder.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/sender/denm_sender.h"
#include "vanetza_integration/denm/service/denm_service.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"

#include "ns3/command-line.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-module.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "vanetza_integration/tests/denm_test_pki.h"

namespace
{

bool
Check(bool condition, const std::string& message)
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
    const std::string& vehicleId,
    const std::string& eventId)
{
    ns3::DenmTriggerRequest request;

    request.source =
        ns3::DenmTriggerSource::TEST;

    request.eventType =
        ns3::DenmTriggerEventType::HAZARD_DETECTED;

    request.vehicleId =
        vehicleId;

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
        "MULTI_VEHICLE_V3_RUNTIME_SIGN";

    return request;
}

bool
GenerateAndSend(
    ns3::VehicleState& vehicle,
    ns3::DenmLifecycleManager& lifecycle,
    const ns3::DenmTriggerRequest& request,
    std::shared_ptr<ns3::PacketContext>& outContext)
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
main(int argc, char* argv[])
{
    std::cout
        << "===== DENM MULTI-VEHICLE V3 RUNTIME SIGN TEST =====\n";

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
            << "FAIL --itsStartTime is required for DENM V3 test"
            << std::endl;
        return 1;
    }

    SimulationContext::itsStartTime = itsStartTime;

    std::cout
        << "ITS_START_TIME="
        << SimulationContext::itsStartTime
        << std::endl;


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

    const std::vector<std::string> vehicleIds{
        "veh0",
        "veh1",
        "veh2",
        "veh3"
    };

    bool ok = true;

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        pkiDir);

    ns3::DenmLifecycleManager lifecycle;
    lifecycle.Reset();

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

    std::vector<ns3::VehicleState*> vehicles;

    for (const auto& vehicleId : vehicleIds)
    {
        ns3::VehicleFactory::CreateVehicle(vehicleId);

        auto* vehicle =
            registry.GetByVehId(vehicleId);

        ok &= Check(
            vehicle != nullptr,
            vehicleId + " is created through the real VehicleFactory");

        if (vehicle == nullptr)
        {
            return 1;
        }

        vehicles.push_back(vehicle);

        ok &= Check(
            vehicle->runtime != nullptr &&
                vehicle->runtime->IsInitialized(),
            vehicleId + " VanetzaRuntime is initialized");

        ok &= Check(
            vehicle->security.credentialPool.Size() >= 3,
            vehicleId + " has at least three DENM credentials");

        ok &= Check(
            vehicle->security.credentialPool.GetActiveCredentialIndex() == 0,
            vehicleId + " starts with credential 000 active");

        ok &= Check(
            vehicle->security.certFile.find(
                vehicleId + "-ticket-000.cert") !=
                std::string::npos,
            vehicleId + " starts with its own credential 000");
    }

    std::vector<std::shared_ptr<ns3::PacketContext>> contexts(
        vehicleIds.size());

    ns3::Simulator::Schedule(
        ns3::Seconds(1.0),
        [&]()
        {
            for (std::size_t i = 0; i < vehicles.size(); ++i)
            {
                const auto request =
                    MakeRequest(
                        vehicleIds[i],
                        "multi-v3-" + vehicleIds[i]);

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        request,
                        contexts[i]),
                    vehicleIds[i] +
                        " traverses trigger -> lifecycle -> builder -> service -> sender");

                ok &= Check(
                    contexts[i] != nullptr,
                    vehicleIds[i] +
                        " retains PacketContext after DENM send");

                if (contexts[i])
                {
                    ok &= Check(
                        !contexts[i]->signerCertificateIdHex.empty(),
                        vehicleIds[i] +
                            " exposes a real V3 signer certificate identity");
                }
            }
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(1.5),
        [&]()
        {
            std::set<std::string> signerIds;

            for (std::size_t i = 0; i < contexts.size(); ++i)
            {
                if (!contexts[i])
                {
                    ok = false;
                    continue;
                }

                ok &= Check(
                    contexts[i]->pipelineStage ==
                        ns3::PipelineStage::TX_COMPLETE,
                    vehicleIds[i] +
                        " reaches TX_COMPLETE");

                ok &= Check(
                    contexts[i]->pipelineResult ==
                        ns3::PipelineResult::OK,
                    vehicleIds[i] +
                        " TX_COMPLETE result is OK");

                signerIds.insert(
                    contexts[i]->signerCertificateIdHex);

                std::cout
                    << "SIGNER "
                    << vehicleIds[i]
                    << "="
                    << contexts[i]->signerCertificateIdHex
                    << "\n";



            }

            ok &= Check(
                signerIds.size() == vehicleIds.size(),
                "all four vehicles use distinct V3 signer certificate identities");

            std::cout
                << "UNIQUE_SIGNER_COUNT="
                << signerIds.size()
                << "\n";
        });

    ns3::Simulator::Stop(
        ns3::Seconds(2.0));

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    const int rc = ok ? 0 : 1;

    std::cout
        << "DENM_MULTI_VEHICLE_V3_RUNTIME_SIGN_EXIT="
        << rc
        << "\n";

    return rc;
}
