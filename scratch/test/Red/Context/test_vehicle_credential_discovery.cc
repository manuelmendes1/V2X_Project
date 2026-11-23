#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"
#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/rx/rx_callback/rx_callback.h"

#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/its_time/its_start_time_parser.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/context/rx_bridge/rx_bridge_context.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/builder/denm_builder.h"
#include "vanetza_integration/denm/service/denm_service.h"
#include "vanetza_integration/denm/sender/denm_sender.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_verify_service.h"
#include <vanetza/common/manual_runtime.hpp>

#include <cstdint>

namespace
{

std::string
CurrentUtcItsStartTime()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t value =
        std::chrono::system_clock::to_time_t(now);

    std::tm utc{};

    if (gmtime_r(&value, &utc) == nullptr)
    {
        throw std::runtime_error("Unable to obtain UTC clock");
    }

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

} // namespace

int main(int argc, char** argv)
{

    if (argc == 4 &&
        std::string(argv[1]) == "--denm-trust-config")
    {
        using namespace ns3;

        const std::string runDirectory = argv[2];
        const std::string expectedText = argv[3];

        if (expectedText != "accept" && expectedText != "reject")
        {
            std::cerr << "DENM_TRUST_CONFIG_INVALID_EXPECTATION\n";
            return 2;
        }

        vanetza::ManualRuntime runtime;
        bool constructed = false;

        try
        {
            DenmV3VerifyService verifier(runtime, runDirectory);
            constructed = true;
        }
        catch (const std::exception& error)
        {
            std::cout
                << "DENM_TRUST_CONFIG_EXCEPTION="
                << error.what() << '\n';
        }

        const bool expectedAccept = expectedText == "accept";
        const bool contract = constructed == expectedAccept;

        std::cout
            << "DENM_TRUST_CONFIG_EXPECTED="
            << (expectedAccept ? "ACCEPT" : "REJECT") << '\n'
            << "DENM_TRUST_CONFIG_ACTUAL="
            << (constructed ? "ACCEPT" : "REJECT") << '\n'
            << "DENM_TRUST_CONFIG_RESULT="
            << (contract ? "PASS" : "FAIL") << '\n';

        return contract ? 0 : 1;
    }

    if (argc == 2 && std::string(argv[1]) == "--rx-bridge-contract")
    {
        using namespace ns3;

        PacketContext outerContext{};
        PacketContext innerContext{};

        const bool initiallyEmpty =
            RxBridgeContext::Current() == nullptr;

        RxBridgeContext::Push(&outerContext);

        const bool outerContextBound =
            RxBridgeContext::Current() == &outerContext;

        RxBridgeContext::Push(&innerContext);

        const bool innerContextBound =
            RxBridgeContext::Current() == &innerContext;

        RxBridgeContext::Pop();

        const bool nestedContextRestored =
            RxBridgeContext::Current() == &outerContext;

        RxBridgeContext::Pop();

        const bool emptyAfterNestedScopes =
            RxBridgeContext::Current() == nullptr;

        // Simulate exception unwinding while a nested context is active.
        RxBridgeContext::Push(&outerContext);

        try
        {
            RxBridgeContext::Push(&innerContext);
            throw std::runtime_error("RX_BRIDGE_TEST_EXCEPTION");
        }
        catch (const std::runtime_error&)
        {
            RxBridgeContext::Pop();
        }

        const bool exceptionContextRestored =
            RxBridgeContext::Current() == &outerContext;

        RxBridgeContext::Pop();

        const bool emptyAfterException =
            RxBridgeContext::Current() == nullptr;

        std::cout
            << "RX_BRIDGE_INITIAL_EMPTY="
            << (initiallyEmpty ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_OUTER_BOUND="
            << (outerContextBound ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_INNER_BOUND="
            << (innerContextBound ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_NESTED_RESTORE="
            << (nestedContextRestored ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_EMPTY_AFTER_NESTED="
            << (emptyAfterNestedScopes ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_EXCEPTION_RESTORE="
            << (exceptionContextRestored ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_EMPTY_AFTER_EXCEPTION="
            << (emptyAfterException ? "PASS" : "FAIL") << '\n';

        bool scopeOuterBound = false;
        bool scopeInnerBound = false;
        bool scopeNestedRestore = false;
        bool scopeEmptyAfterExit = false;

        {
            RxBridgeContext::Scope outerScope(&outerContext);
            scopeOuterBound =
                RxBridgeContext::Current() == &outerContext;

            {
                RxBridgeContext::Scope innerScope(&innerContext);
                scopeInnerBound =
                    RxBridgeContext::Current() == &innerContext;
            }

            scopeNestedRestore =
                RxBridgeContext::Current() == &outerContext;
        }

        scopeEmptyAfterExit =
            RxBridgeContext::Current() == nullptr;

        bool scopeExceptionRestore = false;
        bool scopeExceptionEmpty = false;

        {
            RxBridgeContext::Scope outerScope(&outerContext);

            try
            {
                RxBridgeContext::Scope innerScope(&innerContext);
                throw std::runtime_error(
                    "RX_BRIDGE_SCOPE_EXCEPTION_TEST");
            }
            catch (const std::runtime_error&)
            {
                scopeExceptionRestore =
                    RxBridgeContext::Current() == &outerContext;
            }
        }

        scopeExceptionEmpty =
            RxBridgeContext::Current() == nullptr;

        std::cout
            << "RX_BRIDGE_SCOPE_OUTER_BOUND="
            << (scopeOuterBound ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_SCOPE_INNER_BOUND="
            << (scopeInnerBound ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_SCOPE_NESTED_RESTORE="
            << (scopeNestedRestore ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_SCOPE_EMPTY_AFTER_EXIT="
            << (scopeEmptyAfterExit ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_SCOPE_EXCEPTION_RESTORE="
            << (scopeExceptionRestore ? "PASS" : "FAIL") << '\n'
            << "RX_BRIDGE_SCOPE_EXCEPTION_EMPTY="
            << (scopeExceptionEmpty ? "PASS" : "FAIL") << '\n';

        const bool contract =
            initiallyEmpty &&
            outerContextBound &&
            innerContextBound &&
            nestedContextRestored &&
            emptyAfterNestedScopes &&
            exceptionContextRestored &&
            emptyAfterException &&
            scopeOuterBound &&
            scopeInnerBound &&
            scopeNestedRestore &&
            scopeEmptyAfterExit &&
            scopeExceptionRestore &&
            scopeExceptionEmpty;

        std::cout
            << "RX_BRIDGE_NESTED_CONTEXT_CONTRACT="
            << (contract ? "PASS" : "FAIL") << '\n';

        return contract ? 0 : 1;
    }

    if (argc == 2 &&
        std::string(argv[1]) == "--vehicle-registry-clear-contract")
    {
        using namespace ns3;

        auto& registry = VehicleRegistry::Instance();
        registry.Clear();

        VehicleState state{};

        const GeoNetMid mid{
            {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
        };

        registry.RegisterMid(mid, state);

        const bool midRegistered =
            registry.GetByMid(mid) == &state;

        registry.Clear();

        const bool midIndexCleared =
            registry.GetByMid(mid) == nullptr;

        std::cout
            << "VEHICLE_REGISTRY_MID_REGISTERED="
            << (midRegistered ? "PASS" : "FAIL") << '\n'
            << "VEHICLE_REGISTRY_MID_CLEARED="
            << (midIndexCleared ? "PASS" : "FAIL") << '\n';

        const bool contract =
            midRegistered &&
            midIndexCleared;

        std::cout
            << "VEHICLE_REGISTRY_CLEAR_CONTRACT="
            << (contract ? "PASS" : "FAIL") << '\n';

        return contract ? 0 : 1;
    }

    if (argc == 4 && std::string(argv[1]) == "--real-denm-rx")
    {
        using namespace ns3;

        const std::string runDirectory = argv[2];
        const std::string mappingFile = argv[3];

        std::string configurationError;

        const bool configured =
            VehicleSecurityInitializer::ConfigureVehiclePki(
                runDirectory,
                mappingFile,
                configurationError);

        std::cout
            << "REAL_DENM_RX_PKI_CONFIG="
            << (configured ? "PASS" : "FAIL") << '\n';

        if (!configured)
        {
            std::cerr
                << "REAL_DENM_RX_PKI_CONFIG_ERROR="
                << configurationError << '\n';
            return 10;
        }

        // Use the same UTC origin for both Vanetza runtimes and DENM.
        const std::string startTimeUtc = CurrentUtcItsStartTime();

        SimulationContext::simulationStep = 0;
        SimulationContext::itsStartTime = startTimeUtc;
        SimulationContext::cpmPkiDirectory.clear();

        vanetza::Clock::time_point itsStartTime{};
        std::string timeParseError;

        const bool timeParsed =
            ItsStartTimeParser::ParseUtc(
                startTimeUtc,
                itsStartTime,
                timeParseError);

        const auto triggerTimestampUs =
            timeParsed
                ? std::chrono::duration_cast<std::chrono::microseconds>(
                      itsStartTime.time_since_epoch())
                      .count()
                : 0;

        const bool itsTimeReady =
            timeParsed && triggerTimestampUs > 0;

        std::cout
            << "REAL_DENM_RX_ITS_TIME="
            << (itsTimeReady ? "PASS" : "FAIL") << '\n';

        if (!itsTimeReady)
        {
            std::cerr
                << "REAL_DENM_RX_ITS_TIME_ERROR="
                << timeParseError << '\n';
            return 11;
        }

        // Shared production 802.11p channel and advancing IPv4 allocator.
        YansWifiChannelHelper channelHelper;

        channelHelper.SetPropagationDelay(
            "ns3::ConstantSpeedPropagationDelayModel");

        channelHelper.AddPropagationLoss(
            "ns3::LogDistancePropagationLossModel",
            "Exponent",
            DoubleValue(2.7),
            "ReferenceDistance",
            DoubleValue(1.0));

        Ptr<YansWifiChannel> channel = channelHelper.Create();

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");

        VehicleFactory::Initialize(
            channel,
            ipv4,
            MakeCallback(&RxCallback::Receive));

        // Both vehicles use the real factory and strict PKI configuration.
        Ptr<Node> senderNode =
            VehicleFactory::CreateVehicle("0");

        Ptr<Node> receiverNode =
            VehicleFactory::CreateVehicle("1");

        VehicleState* senderState =
            VehicleRegistry::Instance().GetByVehId("0");

        VehicleState* receiverState =
            VehicleRegistry::Instance().GetByVehId("1");

        const VehicleCredential* senderCredential =
            senderState != nullptr
                ? senderState->security.credentialPool.GetActiveCredential()
                : nullptr;

        const VehicleCredential* receiverCredential =
            receiverState != nullptr
                ? receiverState->security.credentialPool.GetActiveCredential()
                : nullptr;

        const std::string canonicalRunDirectory =
            std::filesystem::canonical(runDirectory).string();

        const auto* senderEndpoint =
            senderState != nullptr
                ? StationEndpointRegistry::Instance().GetByIpv4(
                      senderState->ipv4Address)
                : nullptr;

        const auto* receiverEndpoint =
            receiverState != nullptr
                ? StationEndpointRegistry::Instance().GetByIpv4(
                      receiverState->ipv4Address)
                : nullptr;

        const bool senderReady =
            senderNode != nullptr &&
            senderState != nullptr &&
            senderState->node == senderNode &&
            senderState->identity.vehId == "0" &&
            senderState->identity.nodeId == senderNode->GetId() &&
            senderState->identity.stationId != 0 &&
            senderState->runtime != nullptr &&
            senderState->runtime->IsInitialized() &&
            senderState->socket != nullptr &&
            senderState->linkMacInitialized &&
            senderState->ipv4Initialized;

        const bool receiverReady =
            receiverNode != nullptr &&
            receiverState != nullptr &&
            receiverState->node == receiverNode &&
            receiverState->identity.vehId == "1" &&
            receiverState->identity.nodeId == receiverNode->GetId() &&
            receiverState->identity.stationId != 0 &&
            receiverState->runtime != nullptr &&
            receiverState->runtime->IsInitialized() &&
            receiverState->socket != nullptr &&
            receiverState->linkMacInitialized &&
            receiverState->ipv4Initialized;

        const bool endpointIndexesCorrect =
            senderEndpoint != nullptr &&
            receiverEndpoint != nullptr &&
            senderEndpoint->vehicleId == "0" &&
            receiverEndpoint->vehicleId == "1" &&
            senderEndpoint->stationId == senderState->identity.stationId &&
            receiverEndpoint->stationId == receiverState->identity.stationId &&
            senderEndpoint->nodeId == senderNode->GetId() &&
            receiverEndpoint->nodeId == receiverNode->GetId();

        const bool twoVehiclesReady =
            senderReady &&
            receiverReady &&
            endpointIndexesCorrect &&
            senderState->identity.stationId !=
                receiverState->identity.stationId &&
            senderState->identity.nodeId !=
                receiverState->identity.nodeId &&
            senderState->ipv4Address !=
                receiverState->ipv4Address &&
            senderState->linkMac != receiverState->linkMac &&
            senderState->runtime.get() != receiverState->runtime.get() &&
            VehicleRegistry::Instance().Size() == 2;

        std::cout
            << "REAL_DENM_RX_TWO_VEHICLES="
            << (twoVehiclesReady ? "PASS" : "FAIL") << '\n';

        const bool credentialsCorrect =
            senderCredential != nullptr &&
            receiverCredential != nullptr &&
            senderState->security.credentialPool.Size() == 1 &&
            receiverState->security.credentialPool.Size() == 1 &&
            senderCredential->credentialId == "veh0-ticket-000" &&
            receiverCredential->credentialId == "veh1-ticket-000" &&
            senderCredential->vehicleId == "0" &&
            receiverCredential->vehicleId == "1" &&
            senderCredential->certFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.cert") &&
            senderCredential->keyFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.pem") &&
            receiverCredential->certFile.ends_with(
                "/issued-tickets/dual-aid/veh1-ticket-000.cert") &&
            receiverCredential->keyFile.ends_with(
                "/issued-tickets/dual-aid/veh1-ticket-000.pem") &&
            senderState->security.vehiclePkiRunDirectory ==
                canonicalRunDirectory &&
            receiverState->security.vehiclePkiRunDirectory ==
                canonicalRunDirectory;

        std::cout
            << "REAL_DENM_RX_CREDENTIALS="
            << (credentialsCorrect ? "PASS" : "FAIL") << '\n';

        // Put both radio endpoints at the same simulation position.
        Ptr<ConstantVelocityMobilityModel> senderMobility;
        Ptr<ConstantVelocityMobilityModel> receiverMobility;

        if (senderNode != nullptr)
        {
            senderMobility =
                senderNode->GetObject<ConstantVelocityMobilityModel>();
        }

        if (receiverNode != nullptr)
        {
            receiverMobility =
                receiverNode->GetObject<ConstantVelocityMobilityModel>();
        }

        if (senderMobility != nullptr)
        {
            senderMobility->SetPosition(Vector(0.0, 0.0, 0.0));
            senderMobility->SetVelocity(Vector(0.0, 0.0, 0.0));
        }

        if (receiverMobility != nullptr)
        {
            receiverMobility->SetPosition(Vector(0.0, 0.0, 0.0));
            receiverMobility->SetVelocity(Vector(0.0, 0.0, 0.0));
        }

        const bool mobilityReady =
            senderMobility != nullptr &&
            receiverMobility != nullptr &&
            senderMobility->GetPosition().x == 0.0 &&
            senderMobility->GetPosition().y == 0.0 &&
            receiverMobility->GetPosition().x == 0.0 &&
            receiverMobility->GetPosition().y == 0.0;

        std::cout
            << "REAL_DENM_RX_MOBILITY="
            << (mobilityReady ? "PASS" : "FAIL") << '\n';

        DenmLifecycleManager lifecycle;
        DenmLifecycleResult lifecycleResult{};

        DenmTriggerRequest trigger{};
        trigger.source = DenmTriggerSource::TEST;
        trigger.eventType = DenmTriggerEventType::HAZARD_DETECTED;
        trigger.vehicleId = "0";
        trigger.eventId = "real-pki-denm-v3-rx";
        trigger.x = 0.0;
        trigger.y = 0.0;
        trigger.causeCode = 2;
        trigger.relevanceDistanceMeters = 100.0;
        trigger.validityDurationMs = 5000;
        trigger.triggerTimestampUs =
            static_cast<std::uint64_t>(triggerTimestampUs);
        trigger.reason = "REAL_PKI_DENM_V3_RX_TEST";

        const bool lifecycleAccepted =
            twoVehiclesReady &&
            credentialsCorrect &&
            mobilityReady &&
            lifecycle.Process(
                *senderState,
                trigger,
                lifecycleResult);

        std::cout
            << "REAL_DENM_RX_LIFECYCLE="
            << (lifecycleAccepted ? "PASS" : "FAIL") << '\n';

        const bool actionIdCorrect =
            lifecycleAccepted &&
            lifecycleResult.action == DenmManagementAction::NEW &&
            lifecycleResult.actionId.originatingStationId ==
                senderState->identity.stationId &&
            lifecycleResult.actionId.sequenceNumber != 0;

        std::cout
            << "REAL_DENM_RX_ACTION_ID="
            << (actionIdCorrect ? "PASS" : "FAIL") << '\n';

        DenmGenerationContext generation{};

        if (actionIdCorrect)
        {
            generation =
                DenmBuilder::Build(
                    *senderState,
                    trigger,
                    lifecycleResult);
        }

        const std::uint64_t expectedItsMs =
            static_cast<std::uint64_t>(triggerTimestampUs) / 1000ULL;

        const bool generationBuilt =
            actionIdCorrect &&
            generation.packetContext != nullptr &&
            generation.denmInput.action == DenmManagementAction::NEW &&
            generation.denmInput.actionId.originatingStationId ==
                senderState->identity.stationId &&
            generation.denmInput.actionId.sequenceNumber != 0 &&
            generation.denmInput.detectionTimeItsMs == expectedItsMs &&
            generation.denmInput.referenceTimeItsMs == expectedItsMs &&
            generation.denmInput.eventType.causeCode == 2 &&
            !generation.denmInput.validityDurationMs.has_value() &&
            !generation.denmInput.transmissionIntervalMs.has_value();

        std::cout
            << "REAL_DENM_RX_GENERATION="
            << (generationBuilt ? "PASS" : "FAIL") << '\n';

        std::vector<std::uint8_t> rawPayload;

        if (generationBuilt)
        {
            rawPayload =
                DenmService::GenerateRawPayload(
                    generation,
                    trigger.reason);
        }

        const bool payloadGenerated = !rawPayload.empty();

        std::cout
            << "REAL_DENM_RX_UPER_PAYLOAD="
            << (payloadGenerated ? "PASS" : "FAIL") << '\n'
            << "REAL_DENM_RX_UPER_PAYLOAD_BYTES="
            << rawPayload.size() << '\n';

        bool routerAccepted = false;

        if (payloadGenerated)
        {
            routerAccepted =
                DenmSender::SendRaw(
                    generation.packetContext,
                    *senderState,
                    generation.denmInput,
                    rawPayload,
                    trigger.relevanceDistanceMeters,
                    trigger.reason);
        }

        std::cout
            << "REAL_DENM_RX_ROUTER_ACCEPTED="
            << (routerAccepted ? "PASS" : "FAIL") << '\n';

        const bool signerCredentialIdObserved =
            routerAccepted &&
            generation.packetContext != nullptr &&
            generation.packetContext->signerCertificateIdHex.size() == 16;

        std::cout
            << "REAL_DENM_RX_SIGNER_CREDENTIAL_ID="
            << (signerCredentialIdObserved ? "PASS" : "FAIL") << '\n';

        if (routerAccepted)
        {
            // The DCC scheduler deferred the previous single-vehicle test
            // by about 3.7 seconds. Leave adequate time for TX and RX.
            Simulator::Stop(Seconds(8.0));
            Simulator::Run();
        }

        const bool txComplete =
            routerAccepted &&
            generation.packetContext != nullptr &&
            generation.packetContext->pipelineStage ==
                PipelineStage::TX_COMPLETE &&
            generation.packetContext->pipelineResult ==
                PipelineResult::OK;

        std::cout
            << "REAL_DENM_RX_TX_COMPLETE="
            << (txComplete ? "PASS" : "FAIL") << '\n';

        if (generation.packetContext != nullptr)
        {
            std::cout
                << "REAL_DENM_RX_FINAL_PIPELINE_STAGE="
                << static_cast<unsigned>(
                       generation.packetContext->pipelineStage)
                << '\n'
                << "REAL_DENM_RX_FINAL_PIPELINE_RESULT="
                << static_cast<unsigned>(
                       generation.packetContext->pipelineResult)
                << '\n';
        }

        const bool harnessContract =
            configured &&
            itsTimeReady &&
            twoVehiclesReady &&
            credentialsCorrect &&
            mobilityReady &&
            lifecycleAccepted &&
            actionIdCorrect &&
            generationBuilt &&
            payloadGenerated &&
            routerAccepted &&
            signerCredentialIdObserved &&
            txComplete;

        std::cout
            << "REAL_DENM_RX_HARNESS_CONTRACT="
            << (harnessContract ? "PASS" : "FAIL") << '\n';

        // Cleanup: release protocol runtimes before their VehicleStates.
        if (lifecycleAccepted)
        {
            lifecycle.Abort(
                trigger.vehicleId,
                trigger.eventId);
        }

        if (generation.packetContext != nullptr &&
            TxBridgeContext::Current() == generation.packetContext)
        {
            TxBridgeContext::Pop();
        }

        if (senderState != nullptr && senderState->runtime)
        {
            senderState->runtime->Shutdown();
            senderState->runtime.reset();
        }

        if (receiverState != nullptr && receiverState->runtime)
        {
            receiverState->runtime->Shutdown();
            receiverState->runtime.reset();
        }

        if (senderState != nullptr && senderState->socket)
        {
            senderState->socket->Close();
        }

        if (receiverState != nullptr && receiverState->socket)
        {
            receiverState->socket->Close();
        }

        // VehicleRegistry::Clear() does not clear this independent registry.
        StationEndpointRegistry::Instance().Clear();
        VehicleRegistry::Instance().Clear();

        senderNode = nullptr;
        receiverNode = nullptr;
        channel = nullptr;

        Simulator::Destroy();

        return harnessContract ? 0 : 1;
    }

    if (argc == 4 && std::string(argv[1]) == "--real-denm-signature")
    {
        using namespace ns3;

        const std::string runDirectory = argv[2];
        const std::string mappingFile = argv[3];

        std::string configurationError;

        const bool configured =
            VehicleSecurityInitializer::ConfigureVehiclePki(
                runDirectory,
                mappingFile,
                configurationError);

        std::cout << "REAL_DENM_PKI_CONFIG="
                  << (configured ? "PASS" : "FAIL") << '\n';

        if (!configured)
        {
            std::cerr << "REAL_DENM_PKI_CONFIG_ERROR="
                      << configurationError << '\n';
            return 10;
        }

        // Use one identical UTC string for Runtime and DENM timestamps.
        const std::string startTimeUtc = CurrentUtcItsStartTime();

        SimulationContext::simulationStep = 0;
        SimulationContext::itsStartTime = startTimeUtc;
        SimulationContext::cpmPkiDirectory.clear();

        vanetza::Clock::time_point itsStartTime{};
        std::string timeParseError;

        const bool timeParsed =
            ItsStartTimeParser::ParseUtc(
                startTimeUtc,
                itsStartTime,
                timeParseError);

        const auto triggerTimestampUs =
            timeParsed
                ? std::chrono::duration_cast<std::chrono::microseconds>(
                      itsStartTime.time_since_epoch())
                      .count()
                : 0;

        const bool itsTimeReady =
            timeParsed && triggerTimestampUs > 0;

        std::cout << "REAL_DENM_ITS_TIME="
                  << (itsTimeReady ? "PASS" : "FAIL") << '\n';

        if (!itsTimeReady)
        {
            std::cerr << "REAL_DENM_ITS_TIME_ERROR="
                      << timeParseError << '\n';
            return 11;
        }

        // Construct the same production network path as --real-factory.
        YansWifiChannelHelper channelHelper;

        channelHelper.SetPropagationDelay(
            "ns3::ConstantSpeedPropagationDelayModel");

        channelHelper.AddPropagationLoss(
            "ns3::LogDistancePropagationLossModel",
            "Exponent",
            DoubleValue(2.7),
            "ReferenceDistance",
            DoubleValue(1.0));

        Ptr<YansWifiChannel> channel = channelHelper.Create();

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");

        VehicleFactory::Initialize(
            channel,
            ipv4,
            MakeCallback(&RxCallback::Receive));

        Ptr<Node> node = VehicleFactory::CreateVehicle("0");

        VehicleState* state =
            VehicleRegistry::Instance().GetByVehId("0");

        const VehicleCredential* active =
            state != nullptr
                ? state->security.credentialPool.GetActiveCredential()
                : nullptr;

        const bool factoryRuntimeReady =
            node != nullptr &&
            state != nullptr &&
            state->node == node &&
            state->identity.vehId == "0" &&
            state->identity.stationId != 0 &&
            state->runtime != nullptr &&
            state->runtime->IsInitialized() &&
            state->socket != nullptr &&
            state->linkMacInitialized &&
            state->ipv4Initialized &&
            active != nullptr &&
            active->credentialId == "veh0-ticket-000" &&
            active->vehicleId == "0" &&
            active->certFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.cert") &&
            active->keyFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.pem");

        std::cout << "REAL_DENM_FACTORY_RUNTIME="
                  << (factoryRuntimeReady ? "PASS" : "FAIL") << '\n';

        DenmLifecycleManager lifecycle;
        DenmLifecycleResult lifecycleResult{};

        DenmTriggerRequest trigger{};
        trigger.source = DenmTriggerSource::TEST;
        trigger.eventType = DenmTriggerEventType::HAZARD_DETECTED;
        trigger.vehicleId = "0";
        trigger.eventId = "real-pki-denm-signature";
        trigger.x = 0.0;
        trigger.y = 0.0;
        trigger.causeCode = 2;
        trigger.relevanceDistanceMeters = 100.0;
        trigger.validityDurationMs = 5000;
        trigger.triggerTimestampUs =
            static_cast<std::uint64_t>(triggerTimestampUs);
        trigger.reason = "REAL_PKI_DENM_SIGNATURE_TEST";

        const bool lifecycleAccepted =
            factoryRuntimeReady &&
            lifecycle.Process(
                *state,
                trigger,
                lifecycleResult);

        std::cout << "REAL_DENM_LIFECYCLE="
                  << (lifecycleAccepted ? "PASS" : "FAIL") << '\n';

        const bool actionIdCorrect =
            lifecycleAccepted &&
            lifecycleResult.action == DenmManagementAction::NEW &&
            lifecycleResult.actionId.originatingStationId ==
                state->identity.stationId &&
            lifecycleResult.actionId.sequenceNumber != 0;

        std::cout << "REAL_DENM_ACTION_ID="
                  << (actionIdCorrect ? "PASS" : "FAIL") << '\n';

        DenmGenerationContext generation{};

        if (actionIdCorrect)
        {
            generation =
                DenmBuilder::Build(
                    *state,
                    trigger,
                    lifecycleResult);
        }

        const std::uint64_t expectedItsMs =
            static_cast<std::uint64_t>(triggerTimestampUs) / 1000ULL;

        const bool generationBuilt =
            actionIdCorrect &&
            generation.packetContext != nullptr &&
            generation.denmInput.action == DenmManagementAction::NEW &&
            generation.denmInput.actionId.originatingStationId ==
                state->identity.stationId &&
            generation.denmInput.actionId.sequenceNumber != 0 &&
            generation.denmInput.detectionTimeItsMs == expectedItsMs &&
            generation.denmInput.referenceTimeItsMs == expectedItsMs &&
            generation.denmInput.eventType.causeCode == 2 &&
            !generation.denmInput.validityDurationMs.has_value() &&
            !generation.denmInput.transmissionIntervalMs.has_value();

        std::cout << "REAL_DENM_GENERATION="
                  << (generationBuilt ? "PASS" : "FAIL") << '\n';

        std::vector<std::uint8_t> rawPayload;

        if (generationBuilt)
        {
            rawPayload =
                DenmService::GenerateRawPayload(
                    generation,
                    trigger.reason);
        }

        const bool payloadGenerated =
            !rawPayload.empty();

        std::cout << "REAL_DENM_UPER_PAYLOAD="
                  << (payloadGenerated ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_DENM_UPER_PAYLOAD_BYTES="
                  << rawPayload.size() << '\n';

        bool routerAccepted = false;

        if (payloadGenerated)
        {
            routerAccepted =
                DenmSender::SendRaw(
                    generation.packetContext,
                    *state,
                    generation.denmInput,
                    rawPayload,
                    trigger.relevanceDistanceMeters,
                    trigger.reason);
        }

        std::cout << "REAL_DENM_ROUTER_ACCEPTED="
                  << (routerAccepted ? "PASS" : "FAIL") << '\n';

        // The real signer runs synchronously during Router.request().
        // The HashedId8 is retained in the same PacketContext.
        const bool signerCredentialIdObserved =
            routerAccepted &&
            generation.packetContext != nullptr &&
            generation.packetContext->signerCertificateIdHex.size() == 16;

        std::cout << "REAL_DENM_SIGNER_CREDENTIAL_ID="
                  << (signerCredentialIdObserved ? "PASS" : "FAIL")
                  << '\n';

        if (routerAccepted)
        {
            // DCC may defer the packet. Allow the Runtime/FlowControl and
            // Access scheduled send path to complete.
            Simulator::Stop(Seconds(5.0));
            Simulator::Run();
        }

        const bool txComplete =
            routerAccepted &&
            generation.packetContext != nullptr &&
            generation.packetContext->pipelineStage ==
                PipelineStage::TX_COMPLETE &&
            generation.packetContext->pipelineResult ==
                PipelineResult::OK;

        std::cout << "REAL_DENM_TX_COMPLETE="
                  << (txComplete ? "PASS" : "FAIL") << '\n';

        if (generation.packetContext != nullptr)
        {
            std::cout << "REAL_DENM_FINAL_PIPELINE_STAGE="
                      << static_cast<unsigned>(
                             generation.packetContext->pipelineStage)
                      << '\n';

            std::cout << "REAL_DENM_FINAL_PIPELINE_RESULT="
                      << static_cast<unsigned>(
                             generation.packetContext->pipelineResult)
                      << '\n';

            std::cout << "REAL_DENM_SIGNER_ID_LENGTH="
                      << generation.packetContext->signerCertificateIdHex.size()
                      << '\n';
        }

        const bool contract =
            configured &&
            itsTimeReady &&
            factoryRuntimeReady &&
            lifecycleAccepted &&
            actionIdCorrect &&
            generationBuilt &&
            payloadGenerated &&
            routerAccepted &&
            signerCredentialIdObserved &&
            txComplete;

        std::cout << "REAL_DENM_END_TO_END_CONTRACT="
                  << (contract ? "PASS" : "FAIL") << '\n';

        // Isolated test cleanup.
        if (lifecycleAccepted)
        {
            lifecycle.Abort(
                trigger.vehicleId,
                trigger.eventId);
        }

        if (generation.packetContext != nullptr &&
            TxBridgeContext::Current() == generation.packetContext)
        {
            TxBridgeContext::Pop();
        }

        if (state != nullptr && state->runtime)
        {
            state->runtime->Shutdown();
            state->runtime.reset();
        }

        VehicleRegistry::Instance().Clear();

        node = nullptr;
        channel = nullptr;

        Simulator::Destroy();

        return contract ? 0 : 1;
    }

    if (argc == 4 && std::string(argv[1]) == "--real-factory")
    {
        using namespace ns3;

        const std::string runDirectory = argv[2];
        const std::string mappingFile = argv[3];

        std::string configurationError;

        const bool configured =
            VehicleSecurityInitializer::ConfigureVehiclePki(
                runDirectory,
                mappingFile,
                configurationError);

        std::cout << "REAL_FACTORY_PKI_CONFIG="
                  << (configured ? "PASS" : "FAIL") << '\n';

        if (!configured)
        {
            std::cerr << "REAL_FACTORY_PKI_CONFIG_ERROR="
                      << configurationError << '\n';
            return 10;
        }

        SimulationContext::simulationStep = 0;
        SimulationContext::itsStartTime = CurrentUtcItsStartTime();
        SimulationContext::cpmPkiDirectory.clear();

        YansWifiChannelHelper channelHelper;

        channelHelper.SetPropagationDelay(
            "ns3::ConstantSpeedPropagationDelayModel");

        channelHelper.AddPropagationLoss(
            "ns3::LogDistancePropagationLossModel",
            "Exponent",
            DoubleValue(2.7),
            "ReferenceDistance",
            DoubleValue(1.0));

        Ptr<YansWifiChannel> channel = channelHelper.Create();

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");

        VehicleFactory::Initialize(
            channel,
            ipv4,
            MakeCallback(&RxCallback::Receive));

        // Exercise the production VehicleFactory -> VehicleBuilder path.
        // A fatal initialization failure is isolated by the Python runner.
        Ptr<Node> node = VehicleFactory::CreateVehicle("0");

        VehicleState* state =
            VehicleRegistry::Instance().GetByVehId("0");

        const bool nodeCreated =
            node != nullptr &&
            state != nullptr &&
            state->node == node;

        const bool identityCorrect =
            nodeCreated &&
            state->identity.vehId == "0" &&
            state->identity.nodeId == node->GetId() &&
            state->identity.stationId != 0;

        const VehicleCredential* active =
            state != nullptr
                ? state->security.credentialPool.GetActiveCredential()
                : nullptr;

        const bool credentialCorrect =
            state != nullptr &&
            active != nullptr &&
            state->security.credentialPool.Size() == 1 &&
            active->credentialId == "veh0-ticket-000" &&
            active->vehicleId == "0" &&
            active->sequence == 0 &&
            active->certFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.cert") &&
            active->keyFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.pem") &&
            state->security.certFile == active->certFile &&
            state->security.keyFile == active->keyFile;

        const bool securityMaterialLoaded =
            state != nullptr &&
            state->security.privateKey != nullptr &&
            state->security.certificate != nullptr;

        const bool trustRunDirectoryCorrect =
            state != nullptr &&
            state->security.vehiclePkiRunDirectory ==
                std::filesystem::canonical(runDirectory).string();


        const bool networkInitialized =
            state != nullptr &&
            state->socket != nullptr &&
            state->linkMacInitialized &&
            state->ipv4Initialized;

        const bool runtimeInitialized =
            state != nullptr &&
            state->runtime != nullptr &&
            state->runtime->IsInitialized();

        std::cout << "REAL_FACTORY_NODE="
                  << (nodeCreated ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_FACTORY_IDENTITY="
                  << (identityCorrect ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_FACTORY_ACTIVE_CREDENTIAL="
                  << (credentialCorrect ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_FACTORY_SECURITY_MATERIAL="
                  << (securityMaterialLoaded ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_FACTORY_TRUST_RUN_DIRECTORY="
                  << (trustRunDirectoryCorrect ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_FACTORY_NETWORK="
                  << (networkInitialized ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_FACTORY_RUNTIME="
                  << (runtimeInitialized ? "PASS" : "FAIL") << '\n';

        if (runtimeInitialized)
        {
            std::cout
                << "REAL_FACTORY_RUNTIME_IS_INITIALIZED=PASS\n";
        }

        const bool contract =
            configured &&
            nodeCreated &&
            identityCorrect &&
            credentialCorrect &&
            securityMaterialLoaded &&
            trustRunDirectoryCorrect &&
            networkInitialized &&
            runtimeInitialized;

        std::cout << "REAL_FACTORY_INTEGRATION_CONTRACT="
                  << (contract ? "PASS" : "FAIL") << '\n';

        // Explicitly release the runtime while its VehicleState is alive.
        if (state != nullptr && state->runtime)
        {
            state->runtime->Shutdown();
            state->runtime.reset();
        }

        VehicleRegistry::Instance().Clear();

        node = nullptr;
        channel = nullptr;

        Simulator::Destroy();

        return contract ? 0 : 1;
    }

    if (argc == 4 && std::string(argv[1]) == "--real-pki")
    {
        const std::string runDirectory = argv[2];
        const std::string mappingFile = argv[3];

        std::string configurationError;

        const bool configured =
            ns3::VehicleSecurityInitializer::ConfigureVehiclePki(
                runDirectory,
                mappingFile,
                configurationError);

        std::cout << "REAL_PKI_CONFIG="
                  << (configured ? "PASS" : "FAIL") << '\n';

        if (!configured)
        {
            std::cerr << "REAL_PKI_CONFIG_ERROR="
                      << configurationError << '\n';
            return 1;
        }

        ns3::VehicleState state;
        state.identity.vehId = "0";

        std::string initializationError;

        const bool initialized =
            ns3::VehicleSecurityInitializer::Initialize(
                state,
                initializationError);

        const ns3::VehicleCredential* active =
            state.security.credentialPool.GetActiveCredential();

        const bool activeCredentialCorrect =
            initialized &&
            active != nullptr &&
            state.security.credentialPool.Size() == 1 &&
            active->credentialId == "veh0-ticket-000" &&
            active->vehicleId == "0" &&
            active->sequence == 0;

        const bool securityMaterialLoaded =
            initialized &&
            state.security.privateKey != nullptr &&
            state.security.certificate != nullptr;

        const bool realCredentialPaths =
            activeCredentialCorrect &&
            active->certFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.cert") &&
            active->keyFile.ends_with(
                "/issued-tickets/dual-aid/veh0-ticket-000.pem") &&
            state.security.certFile == active->certFile &&
            state.security.keyFile == active->keyFile;

        const bool sumoIdentityPreserved =
            state.identity.vehId == "0" &&
            active != nullptr &&
            active->vehicleId == state.identity.vehId;

        std::cout << "REAL_PKI_INITIALIZE="
                  << (initialized ? "PASS" : "FAIL") << '\n';

        std::cout << "REAL_PKI_ACTIVE_CREDENTIAL="
                  << (activeCredentialCorrect ? "PASS" : "FAIL")
                  << '\n';

        std::cout << "REAL_PKI_SECURITY_MATERIAL="
                  << (securityMaterialLoaded ? "PASS" : "FAIL")
                  << '\n';

        std::cout << "REAL_PKI_CREDENTIAL_PATHS="
                  << (realCredentialPaths ? "PASS" : "FAIL")
                  << '\n';

        std::cout << "REAL_PKI_SUMO_IDENTITY="
                  << (sumoIdentityPreserved ? "PASS" : "FAIL")
                  << '\n';

        std::cout << "REAL_PKI_INITIALIZATION_ERROR="
                  << initializationError << '\n';

        const bool contract =
            configured &&
            initialized &&
            activeCredentialCorrect &&
            securityMaterialLoaded &&
            realCredentialPaths &&
            sumoIdentityPreserved;

        std::cout << "REAL_PKI_ACTIVATION_CONTRACT="
                  << (contract ? "PASS" : "FAIL") << '\n';

        return contract ? 0 : 1;
    }

    if (argc != 3)
    {
        std::cerr
            << "USAGE: test_vehicle_credential_discovery "
            << "<credential-directory> <mapping-fixtures-directory>\n";
        return 2;
    }

    const std::string directory = argv[1];
    const std::string mappingDirectory = argv[2];

    const auto credentials =
        ns3::VehicleSecurityInitializer::DiscoverVehicleCredentials(
            directory, "0", "veh0-ticket");

    const auto missing =
        ns3::VehicleSecurityInitializer::DiscoverVehicleCredentials(
            directory, "0", "missing-ticket");

    const auto emptyVehicle =
        ns3::VehicleSecurityInitializer::DiscoverVehicleCredentials(
            directory, "", "veh0-ticket");

    const auto emptyPrefix =
        ns3::VehicleSecurityInitializer::DiscoverVehicleCredentials(
            directory, "0", "");

    const auto legacy =
        ns3::VehicleSecurityInitializer::DiscoverDenmCredentials(
            directory, "0");

    const auto hasCredentialId =
        [&](const std::string& credentialId)
        {
            for (const auto& credential : credentials)
            {
                if (credential.credentialId == credentialId)
                {
                    return true;
                }
            }
            return false;
        };

    const bool symlinkCredentialFilesExcluded =
        !hasCredentialId("veh0-ticket-005") &&
        !hasCredentialId("veh0-ticket-006") &&
        !hasCredentialId("veh0-ticket-007");

    const bool countCorrect = credentials.size() == 2;

    const bool sorted =
        countCorrect &&
        credentials[0].credentialId == "veh0-ticket-000" &&
        credentials[1].credentialId == "veh0-ticket-002" &&
        credentials[0].sequence == 0 &&
        credentials[1].sequence == 2;

    const bool identityPreserved =
        countCorrect &&
        credentials[0].vehicleId == "0" &&
        credentials[1].vehicleId == "0";

    const bool pathsCorrect =
        countCorrect &&
        credentials[0].certFile.ends_with("veh0-ticket-000.cert") &&
        credentials[0].keyFile.ends_with("veh0-ticket-000.pem");

    const bool incompletePairRejected =
        countCorrect;

    const bool legacyCompatible =
        legacy.size() == 1 &&
        legacy.front().credentialId == "0-ticket-001" &&
        legacy.front().vehicleId == "0";

    const bool missingRejected =
        missing.empty();

    const bool emptyInputsRejected =
        emptyVehicle.empty() && emptyPrefix.empty();

    // Valid mapping: comments, blank lines and CRLF are accepted.
    std::vector<ns3::VehicleCredentialMapping> mappings;
    std::string mappingError;

    const bool validMappingLoaded =
        ns3::VehicleSecurityInitializer::LoadVehicleCredentialMappings(
            mappingDirectory + "/valid.tsv",
            mappings,
            mappingError);

    const bool validMappingShape =
        validMappingLoaded &&
        mappings.size() == 2 &&
        mappings[0].sumoVehicleId == "0" &&
        mappings[0].credentialDirectoryRelative ==
            "issued-tickets/dual-aid" &&
        mappings[0].credentialFilePrefix == "veh0-ticket" &&
        mappings[1].sumoVehicleId == "vehicle_1" &&
        mappings[1].credentialDirectoryRelative ==
            "issued-tickets/dual-aid" &&
        mappings[1].credentialFilePrefix == "veh1-ticket";

    const auto rejectsMapping =
        [&](const std::string& filename)
        {
            std::vector<ns3::VehicleCredentialMapping> parsed;
            std::string error;

            const bool loaded =
                ns3::VehicleSecurityInitializer::
                    LoadVehicleCredentialMappings(
                        mappingDirectory + "/" + filename,
                        parsed,
                        error);

            return !loaded && parsed.empty() && !error.empty();
        };

    const bool invalidMappingsRejected =
        rejectsMapping("duplicate.tsv") &&
        rejectsMapping("missing-column.tsv") &&
        rejectsMapping("extra-column.tsv") &&
        rejectsMapping("empty-field.tsv") &&
        rejectsMapping("absolute-directory.tsv") &&
        rejectsMapping("traversal-directory.tsv") &&
        rejectsMapping("unsafe-prefix.tsv") &&
        rejectsMapping("empty.tsv") &&
        rejectsMapping("missing.tsv");

    // Resolve a valid mapping to its canonical directory.
    const std::string runDirectory =
        mappingDirectory + "/pki-run";

    const ns3::VehicleCredentialMapping validDirectoryMapping{
        "0",
        "issued-tickets/dual-aid",
        "veh0-ticket"
    };

    std::string resolvedDirectory;
    std::string resolutionError;

    const bool validDirectoryResolved =
        ns3::VehicleSecurityInitializer::
            ResolveVehicleCredentialDirectory(
                runDirectory,
                validDirectoryMapping,
                resolvedDirectory,
                resolutionError);

    const std::string expectedCanonicalPath =
        std::filesystem::canonical(
            std::filesystem::path(runDirectory) /
            "issued-tickets/dual-aid").string();

    const bool validPathResolution =
        validDirectoryResolved &&
        resolvedDirectory == expectedCanonicalPath &&
        resolutionError.empty();

    const auto rejectsResolution =
        [&](const std::string& root,
            const ns3::VehicleCredentialMapping& mapping)
        {
            std::string resolved = "stale-value";
            std::string error;

            const bool accepted =
                ns3::VehicleSecurityInitializer::
                    ResolveVehicleCredentialDirectory(
                        root,
                        mapping,
                        resolved,
                        error);

            return !accepted && resolved.empty() && !error.empty();
        };

    const ns3::VehicleCredentialMapping symlinkEscapeMapping{
        "0",
        "escape",
        "veh0-ticket"
    };

    const ns3::VehicleCredentialMapping traversalMapping{
        "0",
        "../outside",
        "veh0-ticket"
    };

    const ns3::VehicleCredentialMapping missingDirectoryMapping{
        "0",
        "does-not-exist",
        "veh0-ticket"
    };

    const ns3::VehicleCredentialMapping absoluteDirectoryMapping{
        "0",
        "/etc",
        "veh0-ticket"
    };

    const bool symlinkEscapeRejected =
        rejectsResolution(runDirectory, symlinkEscapeMapping);

    const bool invalidPathInputsRejected =
        rejectsResolution(runDirectory, traversalMapping) &&
        rejectsResolution(runDirectory, missingDirectoryMapping) &&
        rejectsResolution(runDirectory, absoluteDirectoryMapping) &&
        rejectsResolution(
            mappingDirectory + "/not-a-directory",
            validDirectoryMapping) &&
        rejectsResolution(
            mappingDirectory + "/missing-run",
            validDirectoryMapping);

    const bool pathResolutionContract =
        validPathResolution &&
        symlinkEscapeRejected &&
        invalidPathInputsRejected;

    std::string runOnlyError;
    const bool runOnlyRejected =
        !ns3::VehicleSecurityInitializer::ConfigureVehiclePki(
            runDirectory,
            "",
            runOnlyError) &&
        !runOnlyError.empty();

    std::string mapOnlyError;
    const bool mapOnlyRejected =
        !ns3::VehicleSecurityInitializer::ConfigureVehiclePki(
            "",
            mappingDirectory + "/valid.tsv",
            mapOnlyError) &&
        !mapOnlyError.empty();

    std::string validConfigurationError;
    const bool validConfigurationAccepted =
        ns3::VehicleSecurityInitializer::ConfigureVehiclePki(
            runDirectory,
            mappingDirectory + "/valid.tsv",
            validConfigurationError) &&
        validConfigurationError.empty();


    // A failed reconfiguration must preserve the preceding valid config.
    std::string failedReconfigurationError;

    const bool failedReconfigurationRejected =
        !ns3::VehicleSecurityInitializer::ConfigureVehiclePki(
            runDirectory,
            mappingDirectory + "/missing.tsv",
            failedReconfigurationError) &&
        !failedReconfigurationError.empty();

    // Two distinct SUMO vehicles must not share the same
    // canonical credential directory and credential file prefix.
    const std::string sharedCredentialMappingFile =
        mappingDirectory + "/shared-credential.tsv";

    std::ofstream sharedCredentialMapping(sharedCredentialMappingFile);

    if (sharedCredentialMapping)
    {
        sharedCredentialMapping
            << "0\tissued-tickets/dual-aid\tveh0-ticket\n"
            << "vehicle_1\tissued-tickets/dual-aid\tveh0-ticket\n";
    }

    sharedCredentialMapping.close();

    const bool sharedCredentialMappingWritten =
        static_cast<bool>(sharedCredentialMapping);

    std::string sharedCredentialError;

    const bool sharedCredentialReuseRejected =
        sharedCredentialMappingWritten &&
        !ns3::VehicleSecurityInitializer::ConfigureVehiclePki(
            runDirectory,
            sharedCredentialMappingFile,
            sharedCredentialError) &&
        sharedCredentialError.find(
            "credential directory and prefix already assigned") !=
            std::string::npos;

    std::cout
        << "VEHICLE_PKI_SHARED_CREDENTIAL_ERROR="
        << sharedCredentialError << '\n';

    // A rejected candidate must leave the previous valid configuration active.
    ns3::VehicleState preservedConfigurationState;
    preservedConfigurationState.identity.vehId = "vehicle_1";

    std::string preservedConfigurationError;

    const bool previousConfigurationPreserved =
        !ns3::VehicleSecurityInitializer::Initialize(
            preservedConfigurationState,
            preservedConfigurationError) &&
        preservedConfigurationError.find("no credential files") !=
            std::string::npos &&
        preservedConfigurationState.security.credentialPool.Empty() &&
        preservedConfigurationState.security.privateKey == nullptr &&
        preservedConfigurationState.security.certificate == nullptr &&
        preservedConfigurationState.security.certFile.empty() &&
        preservedConfigurationState.security.keyFile.empty();

    std::cout
        << "VEHICLE_PKI_PREVIOUS_CONFIG_PRESERVED="
        << (previousConfigurationPreserved ? "PASS" : "FAIL")
        << '\n';

    // Restore the valid fixture configuration so this test does not
    // contaminate the remaining fail-closed checks.
    std::string restoredConfigurationError;

    const bool validConfigurationRestored =
        ns3::VehicleSecurityInitializer::ConfigureVehiclePki(
            runDirectory,
            mappingDirectory + "/valid.tsv",
            restoredConfigurationError) &&
        restoredConfigurationError.empty();

    const bool pkiConfigurationContract =
        runOnlyRejected &&
        mapOnlyRejected &&
        validConfigurationAccepted &&
        sharedCredentialReuseRejected &&
        previousConfigurationPreserved &&
        validConfigurationRestored;

    std::cout
        << "VEHICLE_PKI_SHARED_CREDENTIAL_REUSE_REJECTED="
        << (sharedCredentialReuseRejected ? "PASS" : "FAIL")
        << '\n';

    std::cout
        << "VEHICLE_PKI_VALID_CONFIG_RESTORED="
        << (validConfigurationRestored ? "PASS" : "FAIL")
        << '\n';

    // SUMO identity without a mapping must fail closed.
    ns3::VehicleState unmappedState;
    unmappedState.identity.vehId = "unmapped_vehicle";

    std::string unmappedError;

    const bool unmappedRejected =
        !ns3::VehicleSecurityInitializer::Initialize(
            unmappedState,
            unmappedError) &&
        unmappedError.find("no mapping") != std::string::npos &&
        unmappedState.security.credentialPool.Empty();

    // Mapped vehicle with an empty credential directory must fail closed.
    ns3::VehicleState missingCredentialsState;
    missingCredentialsState.identity.vehId = "vehicle_1";

    std::string missingCredentialsError;

    const bool missingCredentialsRejected =
        !ns3::VehicleSecurityInitializer::Initialize(
            missingCredentialsState,
            missingCredentialsError) &&
        missingCredentialsError.find("no credential files") !=
            std::string::npos &&
        missingCredentialsState.security.credentialPool.Empty();

    // A discovered but invalid credential pair must not trigger key generation.
    ns3::VehicleState invalidCredentialsState;
    invalidCredentialsState.identity.vehId = "0";

    std::string invalidCredentialsError;

    const bool invalidCredentialsRejected =
        !ns3::VehicleSecurityInitializer::Initialize(
            invalidCredentialsState,
            invalidCredentialsError) &&
        invalidCredentialsError.find(
            "no credential could be activated") != std::string::npos &&
        invalidCredentialsState.security.credentialPool.Empty();

    const auto hasNoLocalSecurityMaterial =
        [](const ns3::VehicleState& state)
        {
            return state.security.privateKey == nullptr &&
                   state.security.certificate == nullptr &&
                   state.security.certFile.empty() &&
                   state.security.keyFile.empty();
        };

    const bool strictNoLocalKeyFallback =
        hasNoLocalSecurityMaterial(unmappedState) &&
        hasNoLocalSecurityMaterial(missingCredentialsState) &&
        hasNoLocalSecurityMaterial(invalidCredentialsState);

    const bool strictInitializationContract =
        failedReconfigurationRejected &&
        unmappedRejected &&
        missingCredentialsRejected &&
        invalidCredentialsRejected &&
        strictNoLocalKeyFallback;

    std::cout << "DISCOVERY_COUNT=" << credentials.size() << '\n';
    std::cout << "DISCOVERY_SORT_ORDER="
              << (sorted ? "PASS" : "FAIL") << '\n';
    std::cout << "DISCOVERY_IDENTITY_PRESERVED="
              << (identityPreserved ? "PASS" : "FAIL") << '\n';
    std::cout << "DISCOVERY_PATHS="
              << (pathsCorrect ? "PASS" : "FAIL") << '\n';
    std::cout << "SYMLINK_CREDENTIAL_FILES_REJECTED="
              << (symlinkCredentialFilesExcluded ? "PASS" : "FAIL")
              << '\n';
    std::cout << "MISSING_PREFIX_REJECTED="
              << (missingRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "EMPTY_INPUTS_REJECTED="
              << (emptyInputsRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "LEGACY_WRAPPER_COMPATIBILITY="
              << (legacyCompatible ? "PASS" : "FAIL") << '\n';
    std::cout << "MAPPING_VALID_ENTRIES="
              << (validMappingShape ? "PASS" : "FAIL") << '\n';
    std::cout << "MAPPING_INVALID_INPUTS_REJECTED="
              << (invalidMappingsRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "MAPPING_CONTRACT="
              << (validMappingShape &&
                  invalidMappingsRejected ? "PASS" : "FAIL")
              << '\n';
    std::cout << "PATH_RESOLUTION_VALID_DIRECTORY="
              << (validPathResolution ? "PASS" : "FAIL") << '\n';
    std::cout << "PATH_RESOLUTION_SYMLINK_ESCAPE_REJECTED="
              << (symlinkEscapeRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "PATH_RESOLUTION_INVALID_INPUTS_REJECTED="
              << (invalidPathInputsRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "PATH_RESOLUTION_CONTRACT="
              << (pathResolutionContract ? "PASS" : "FAIL") << '\n';
    std::cout << "PKI_CONFIG_RUN_ONLY_REJECTED="
              << (runOnlyRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "PKI_CONFIG_MAP_ONLY_REJECTED="
              << (mapOnlyRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "PKI_CONFIG_VALID_PAIR_ACCEPTED="
              << (validConfigurationAccepted ? "PASS" : "FAIL") << '\n';
    std::cout << "PKI_CONFIG_CONTRACT="
              << (pkiConfigurationContract ? "PASS" : "FAIL") << '\n';
    std::cout << "PKI_CONFIG_FAILED_RECONFIG_REJECTED="
              << (failedReconfigurationRejected ? "PASS" : "FAIL")
              << '\n';
    std::cout << "STRICT_UNMAPPED_VEHICLE_REJECTED="
              << (unmappedRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "STRICT_MISSING_CREDENTIALS_REJECTED="
              << (missingCredentialsRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "STRICT_INVALID_CREDENTIALS_REJECTED="
              << (invalidCredentialsRejected ? "PASS" : "FAIL") << '\n';
    std::cout << "STRICT_NO_LOCAL_KEY_FALLBACK="
              << (strictNoLocalKeyFallback ? "PASS" : "FAIL") << '\n';
    std::cout << "STRICT_INITIALIZATION_CONTRACT="
              << (strictInitializationContract ? "PASS" : "FAIL")
              << '\n';
    std::cout << "DISCOVERY_CONTRACT="
              << (countCorrect &&
                  sorted &&
                  identityPreserved &&
                  pathsCorrect &&
                  symlinkCredentialFilesExcluded &&
                  incompletePairRejected &&
                  legacyCompatible &&
                  missingRejected &&
                  emptyInputsRejected
                      ? "PASS" : "FAIL")
              << '\n';

    return countCorrect &&
           sorted &&
           identityPreserved &&
           pathsCorrect &&
           symlinkCredentialFilesExcluded &&
           legacyCompatible &&
           missingRejected &&
           emptyInputsRejected &&
           validMappingShape &&
           invalidMappingsRejected &&
           pathResolutionContract &&
           pkiConfigurationContract &&
           strictInitializationContract ? 0 : 1;
}
