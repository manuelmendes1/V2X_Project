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
        "DENM_HIGH_DENSITY_V3_RUNTIME_SIGN";

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
        << "===== DENM HIGH-DENSITY V3 RUNTIME SIGN TEST =====\n";

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
    std::vector<std::shared_ptr<ns3::PacketContext>> contexts(
        vehicleCount);

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
            vehicleId + " VanetzaRuntime initialized");

        ok &= Check(
            vehicle->security.denmCredentialPool.Size() >= 3,
            vehicleId + " has at least three DENM credentials");

        ok &= Check(
            vehicle->security.denmCredentialPool.GetActiveCredentialIndex() == 0,
            vehicleId + " starts with credential 000");

        ok &= Check(
            vehicle->security.certFile.find(
                vehicleId + "-ticket-000.cert") !=
                std::string::npos,
            vehicleId + " uses its own credential 000");
    }

    for (std::size_t i = 0; i < vehicleCount; ++i)
    {
        const auto delay =
            1.0 + (0.2 * static_cast<double>(i));

        ns3::Simulator::Schedule(
            ns3::Seconds(delay),
            [&, i]()
            {
                const std::string vehicleId =
                    "veh" + std::to_string(i);

                const auto request =
                    MakeRequest(
                        vehicleId,
                        "high-density-" + vehicleId);

                ok &= Check(
                    GenerateAndSend(
                        *vehicles[i],
                        lifecycle,
                        request,
                        contexts[i]),
                    vehicleId +
                        " signs DENM V3 with credential 000");

                if (contexts[i])
                {
                    ok &= Check(
                        !contexts[i]->signerCertificateIdHex.empty(),
                        vehicleId +
                            " exposes real V3 signer certificate identity");
                }
            });
    }

    ns3::Simulator::Schedule(
        ns3::Seconds(
            1.0 + (0.2 * static_cast<double>(vehicleCount - 1)) + 1.5),
        [&]()
        {
            std::set<std::string> signerIds;

            for (std::size_t i = 0; i < vehicleCount; ++i)
            {
                const std::string vehicleId =
                    "veh" + std::to_string(i);

                ok &= Check(
                    contexts[i] != nullptr,
                    vehicleId + " produced a PacketContext");

                if (!contexts[i])
                {
                    continue;
                }

                ok &= Check(
                    contexts[i]->pipelineStage ==
                        ns3::PipelineStage::TX_COMPLETE,
                    vehicleId + " reaches TX_COMPLETE");

                ok &= Check(
                    contexts[i]->pipelineResult ==
                        ns3::PipelineResult::OK,
                    vehicleId + " TX_COMPLETE result is OK");

                signerIds.insert(
                    contexts[i]->signerCertificateIdHex);
            }

            ok &= Check(
                signerIds.size() == vehicleCount,
                "each high-density vehicle has a distinct V3 signer identity");

            std::cout
                << "UNIQUE_SIGNER_COUNT="
                << signerIds.size()
                << "\n";
        });

    const double stopTimeSeconds =
        3.0 +
        0.2 * static_cast<double>(vehicleCount);

    ns3::Simulator::Stop(
        ns3::Seconds(stopTimeSeconds));

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    const int rc =
        ok ? 0 : 1;

    std::cout
        << "DENM_HIGH_DENSITY_V3_RUNTIME_SIGN_EXIT="
        << rc
        << "\n";

    return rc;
}
