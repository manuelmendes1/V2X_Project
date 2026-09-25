#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"

#include "vanetza_integration/denm/builder/denm_builder.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/sender/denm_sender.h"
#include "vanetza_integration/denm/service/denm_service.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"

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

std::size_t
ReadVehicleCount()
{
    const char* env =
        std::getenv("DENM_TEST_VEHICLE_COUNT");

    if (env == nullptr || std::string(env).empty())
    {
        return 10;
    }

    try
    {
        const auto value =
            std::stoul(env);

        if (value == 0)
        {
            throw std::runtime_error("zero");
        }

        return value;
    }
    catch (...)
    {
        std::cerr
            << "FAIL DENM_TEST_VEHICLE_COUNT must be a positive integer\n";
        std::exit(1);
    }
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
main()
{
    std::cout
        << "===== DENM HIGH-DENSITY V3 RUNTIME ROTATION TEST =====\n";

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

    const std::size_t vehicleCount =
        ReadVehicleCount();

    std::cout
        << "VEHICLE_COUNT="
        << vehicleCount
        << "\n";

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

    for (std::size_t i = 0; i < vehicleCount; ++i)
    {
        const std::string vehicleId =
            "veh" + std::to_string(i);

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
            vehicle->security.denmCredentials.size() == 3,
            vehicleId + " owns exactly three DENM credentials");

        ok &= Check(
            vehicle->security.activeDenmCredentialIndex == 0,
            vehicleId + " starts at credential 000");
    }

    std::vector<
        std::vector<std::shared_ptr<ns3::PacketContext>>>
        contexts(
            vehicleCount,
            std::vector<std::shared_ptr<ns3::PacketContext>>(3));

    // Round 0: credential 000.
    ns3::Simulator::Schedule(
        ns3::Seconds(1.0),
        [&]()
        {
            for (std::size_t i = 0; i < vehicleCount; ++i)
            {
                const std::string vehicleId =
                    "veh" + std::to_string(i);

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        MakeRequest(
                            vehicleId,
                            "hd-000-" + vehicleId,
                            "HD_ROTATION_000"),
                        contexts[i][0]),
                    vehicleId +
                        " signs DENM with credential 000");
            }
        });

    // Round 1: credential 001.
    ns3::Simulator::Schedule(
        ns3::Seconds(6.0),
        [&]()
        {
            for (std::size_t i = 0; i < vehicleCount; ++i)
            {
                const std::string vehicleId =
                    "veh" + std::to_string(i);

                ok &= Check(
                    vehicles[i]->runtime->RotateDenmCredential(),
                    vehicleId +
                        " rotates 000 -> 001");

                ok &= Check(
                    vehicles[i]->security.activeDenmCredentialIndex == 1,
                    vehicleId +
                        " active credential index is 001");

                ok &= Check(
                    vehicles[i]->security.certFile.find(
                        vehicleId + "-ticket-001.cert") !=
                        std::string::npos,
                    vehicleId +
                        " active certificate is credential 001");

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        MakeRequest(
                            vehicleId,
                            "hd-001-" + vehicleId,
                            "HD_ROTATION_001"),
                        contexts[i][1]),
                    vehicleId +
                        " signs DENM with credential 001");
            }
        });

    // Round 2: credential 002.
    ns3::Simulator::Schedule(
        ns3::Seconds(11.0),
        [&]()
        {
            for (std::size_t i = 0; i < vehicleCount; ++i)
            {
                const std::string vehicleId =
                    "veh" + std::to_string(i);

                ok &= Check(
                    vehicles[i]->runtime->RotateDenmCredential(),
                    vehicleId +
                        " rotates 001 -> 002");

                ok &= Check(
                    vehicles[i]->security.activeDenmCredentialIndex == 2,
                    vehicleId +
                        " active credential index is 002");

                ok &= Check(
                    vehicles[i]->security.certFile.find(
                        vehicleId + "-ticket-002.cert") !=
                        std::string::npos,
                    vehicleId +
                        " active certificate is credential 002");

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        MakeRequest(
                            vehicleId,
                            "hd-002-" + vehicleId,
                            "HD_ROTATION_002"),
                        contexts[i][2]),
                    vehicleId +
                        " signs DENM with credential 002");
            }
        });

    // Final acceptance.
    ns3::Simulator::Schedule(
        ns3::Seconds(13.5),
        [&]()
        {
            std::set<std::string> globalSignerIds;

            for (std::size_t i = 0; i < vehicleCount; ++i)
            {
                const std::string vehicleId =
                    "veh" + std::to_string(i);

                std::set<std::string> vehicleSignerIds;

                for (std::size_t credential = 0;
                     credential < 3;
                     ++credential)
                {
                    auto& context =
                        contexts[i][credential];

                    ok &= Check(
                        context != nullptr,
                        vehicleId +
                            " credential " +
                            std::to_string(credential) +
                            " produced PacketContext");

                    if (!context)
                    {
                        continue;
                    }

                    ok &= Check(
                        context->pipelineStage ==
                            ns3::PipelineStage::TX_COMPLETE,
                        vehicleId +
                            " credential " +
                            std::to_string(credential) +
                            " reaches TX_COMPLETE");

                    ok &= Check(
                        context->pipelineResult ==
                            ns3::PipelineResult::OK,
                        vehicleId +
                            " credential " +
                            std::to_string(credential) +
                            " TX result is OK");

                    ok &= Check(
                        !context->signerCertificateIdHex.empty(),
                        vehicleId +
                            " credential " +
                            std::to_string(credential) +
                            " has signer certificate identity");

                    vehicleSignerIds.insert(
                        context->signerCertificateIdHex);

                    globalSignerIds.insert(
                        context->signerCertificateIdHex);

                    std::cout
                        << "SIGNER "
                        << vehicleId
                        << " credential="
                        << credential
                        << " id="
                        << context->signerCertificateIdHex
                        << "\n";
                }

                ok &= Check(
                    vehicleSignerIds.size() == 3,
                    vehicleId +
                        " uses three distinct signer identities");
            }

            ok &= Check(
                globalSignerIds.size() ==
                    vehicleCount * 3,
                "all vehicle/credential combinations use distinct signer identities");

            std::cout
                << "EXPECTED_SIGNER_COUNT="
                << vehicleCount * 3
                << "\n";

            std::cout
                << "UNIQUE_GLOBAL_SIGNER_COUNT="
                << globalSignerIds.size()
                << "\n";
        });

    ns3::Simulator::Stop(
        ns3::Seconds(15.0));

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    const int rc =
        ok ? 0 : 1;

    std::cout
        << "DENM_HIGH_DENSITY_V3_RUNTIME_ROTATION_EXIT="
        << rc
        << "\n";

    return rc;
}
