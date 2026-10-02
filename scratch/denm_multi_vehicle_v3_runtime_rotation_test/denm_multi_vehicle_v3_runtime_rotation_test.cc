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
    const std::string& eventId,
    const std::string& reason)
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
        reason;

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
        << "===== DENM MULTI-VEHICLE V3 RUNTIME ROTATION TEST =====\n";

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
            vehicleId + " created through VehicleFactory");

        if (vehicle == nullptr)
        {
            return 1;
        }

        vehicles.push_back(vehicle);

        ok &= Check(
            vehicle->runtime != nullptr &&
                vehicle->runtime->IsInitialized(),
            vehicleId + " runtime initialized");

        ok &= Check(
            vehicle->security.credentialPool.Size() == 3,
            vehicleId + " owns exactly three DENM credentials");

        ok &= Check(
            vehicle->security.credentialPool.GetActiveCredentialIndex() == 0,
            vehicleId + " starts at credential 000");

        ok &= Check(
            vehicle->security.certFile.find(
                vehicleId + "-ticket-000.cert") !=
                std::string::npos,
            vehicleId + " initially uses credential 000");
    }

    std::vector<
        std::vector<std::shared_ptr<ns3::PacketContext>>>
        contexts(
            vehicleIds.size(),
            std::vector<std::shared_ptr<ns3::PacketContext>>(3));

    // Round 0: credential 000.
    ns3::Simulator::Schedule(
        ns3::Seconds(1.0),
        [&]()
        {
            for (std::size_t i = 0; i < vehicles.size(); ++i)
            {
                auto request =
                    MakeRequest(
                        vehicleIds[i],
                        "rotation-000-" + vehicleIds[i],
                        "ROTATION_CREDENTIAL_000");

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        request,
                        contexts[i][0]),
                    vehicleIds[i] +
                        " signs DENM with credential 000");

                if (contexts[i][0])
                {
                    ok &= Check(
                        !contexts[i][0]->signerCertificateIdHex.empty(),
                        vehicleIds[i] +
                            " credential 000 exposes signer identity");
                }
            }
        });

    // Round 1: rotate 000 -> 001.
    ns3::Simulator::Schedule(
        ns3::Seconds(5.2),
        [&]()
        {
            for (std::size_t i = 0; i < vehicles.size(); ++i)
            {
                ok &= Check(
                    vehicles[i]->runtime->RotateDenmCredential(),
                    vehicleIds[i] +
                        " rotates 000 -> 001");

                ok &= Check(
                    vehicles[i]->security.credentialPool.GetActiveCredentialIndex() == 1,
                    vehicleIds[i] +
                        " active credential index is 001");

                ok &= Check(
                    vehicles[i]->security.certFile.find(
                        vehicleIds[i] + "-ticket-001.cert") !=
                        std::string::npos,
                    vehicleIds[i] +
                        " active certificate is credential 001");
            }
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(5.4),
        [&]()
        {
            for (std::size_t i = 0; i < vehicles.size(); ++i)
            {
                auto request =
                    MakeRequest(
                        vehicleIds[i],
                        "rotation-001-" + vehicleIds[i],
                        "ROTATION_CREDENTIAL_001");

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        request,
                        contexts[i][1]),
                    vehicleIds[i] +
                        " signs DENM with credential 001");

                if (contexts[i][1])
                {
                    ok &= Check(
                        !contexts[i][1]->signerCertificateIdHex.empty(),
                        vehicleIds[i] +
                            " credential 001 exposes signer identity");
                }
            }
        });

    // Round 2: rotate 001 -> 002.
    ns3::Simulator::Schedule(
        ns3::Seconds(9.2),
        [&]()
        {
            for (std::size_t i = 0; i < vehicles.size(); ++i)
            {
                ok &= Check(
                    vehicles[i]->runtime->RotateDenmCredential(),
                    vehicleIds[i] +
                        " rotates 001 -> 002");

                ok &= Check(
                    vehicles[i]->security.credentialPool.GetActiveCredentialIndex() == 2,
                    vehicleIds[i] +
                        " active credential index is 002");

                ok &= Check(
                    vehicles[i]->security.certFile.find(
                        vehicleIds[i] + "-ticket-002.cert") !=
                        std::string::npos,
                    vehicleIds[i] +
                        " active certificate is credential 002");
            }
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(9.4),
        [&]()
        {
            for (std::size_t i = 0; i < vehicles.size(); ++i)
            {
                auto request =
                    MakeRequest(
                        vehicleIds[i],
                        "rotation-002-" + vehicleIds[i],
                        "ROTATION_CREDENTIAL_002");

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        request,
                        contexts[i][2]),
                    vehicleIds[i] +
                        " signs DENM with credential 002");

                if (contexts[i][2])
                {
                    ok &= Check(
                        !contexts[i][2]->signerCertificateIdHex.empty(),
                        vehicleIds[i] +
                            " credential 002 exposes signer identity");
                }
            }
        });

    ns3::Simulator::Schedule(
        ns3::Seconds(10.5),
        [&]()
        {
            std::set<std::string> globalSignerIds;

            for (std::size_t i = 0; i < vehicleIds.size(); ++i)
            {
                std::set<std::string> vehicleSignerIds;

                for (std::size_t c = 0; c < 3; ++c)
                {
                    auto& context =
                        contexts[i][c];

                    ok &= Check(
                        context != nullptr,
                        vehicleIds[i] +
                            " credential " +
                            std::to_string(c) +
                            " produced PacketContext");

                    if (!context)
                    {
                        continue;
                    }

                    ok &= Check(
                        context->pipelineStage ==
                            ns3::PipelineStage::TX_COMPLETE,
                        vehicleIds[i] +
                            " credential " +
                            std::to_string(c) +
                            " reaches TX_COMPLETE");

                    ok &= Check(
                        context->pipelineResult ==
                            ns3::PipelineResult::OK,
                        vehicleIds[i] +
                            " credential " +
                            std::to_string(c) +
                            " TX result is OK");

                    std::cout
                        << "SIGNER "
                        << vehicleIds[i]
                        << " credential=" << c
                        << " id="
                        << context->signerCertificateIdHex
                        << "\n";



                    vehicleSignerIds.insert(
                        context->signerCertificateIdHex);

                    globalSignerIds.insert(
                        context->signerCertificateIdHex);
                }

                ok &= Check(
                    vehicleSignerIds.size() == 3,
                    vehicleIds[i] +
                        " uses three distinct signer identities across 000/001/002");
            }

            ok &= Check(
                globalSignerIds.size() == 12,
                "all 12 vehicle/credential combinations use distinct signer identities");

            std::cout
                << "UNIQUE_GLOBAL_SIGNER_COUNT="
                << globalSignerIds.size()
                << "\n";
        });

    ns3::Simulator::Stop(
        ns3::Seconds(11.0));

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    const int rc = ok ? 0 : 1;

    std::cout
        << "DENM_MULTI_VEHICLE_V3_RUNTIME_ROTATION_EXIT="
        << rc
        << "\n";

    return rc;
}
