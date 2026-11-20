#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <vanetza/common/clock.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_fix.hpp>
#include <vanetza/geonet/address.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/net/mac_address.hpp>
#include <vanetza/units/angle.hpp>
#include <vanetza/units/length.hpp>
#include <vanetza/units/time.hpp>
#include <vanetza/units/velocity.hpp>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/wifi-module.h"

#include "vanetza_integration/context/its_station/network/its_station_network_builder.h"
#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/context/tx_bridge/tx_completion.h"
#include "vanetza_integration/vam/transport/vam_transport_gateway.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_dcc_adapter.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"

namespace
{
using namespace ns3;
using CompletionPtr = std::shared_ptr<TxCompletion>;

struct ScenarioObservation
{
    bool submitAccepted{false};
    bool callbackContextMatches{false};
    bool bridgeConsumed{false};
    bool bridgeEmptyAfterSubmit{false};
    unsigned completionCalls{0};
    TxCompletionResult completionResult{};
    std::shared_ptr<PacketContext> context;
    CompletionPtr completion;
};

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    return condition;
}

void ConfigureMib(vanetza::geonet::MIB& mib)
{
    const vanetza::MacAddress localMac{
        0x02, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    vanetza::geonet::Address localAddress(localMac);
    mib.itsGnLocalGnAddr = localAddress;
    mib.itsGnSecurity = false;
    mib.itsGnMaxSduSize = 1400;
    mib.itsGnMaxPacketLifetime.encode(
        vanetza::units::Duration::from_value(10.0));
    mib.itsGnDefaultPacketLifetime.encode(
        vanetza::units::Duration::from_value(1.0));
    mib.vanetzaDisableBeaconing = true;
}

void ConfigurePosition(
    vanetza::geonet::Router& router,
    vanetza::ManualRuntime& runtime)
{
    vanetza::PositionFix position;
    position.timestamp = runtime.now();
    position.latitude = vanetza::units::GeoAngle::from_value(38.7223);
    position.longitude = vanetza::units::GeoAngle::from_value(-9.1393);
    position.confidence.semi_major =
        vanetza::units::Length::from_value(5.0);
    position.confidence.semi_minor =
        vanetza::units::Length::from_value(5.0);
    position.confidence.orientation =
        vanetza::units::TrueNorth::from_value(0.0);
    position.course =
        vanetza::ConfidentQuantity<vanetza::units::TrueNorth>(
            vanetza::units::TrueNorth::from_value(0.0));
    position.speed =
        vanetza::ConfidentQuantity<vanetza::units::Velocity>(
            vanetza::units::Velocity::from_value(0.0));
    router.update_position(position);
}

ScenarioObservation RunScenario(
    bool accessSocketAvailable,
    uint64_t packetId,
    vanetza::Clock::time_point itsStartTime,
    Time submitDelay)
{
    ScenarioObservation observation{};
    TxBridgeContext::Pop();

    {
    YansWifiChannelHelper channelHelper;
    channelHelper.SetPropagationDelay(
        "ns3::ConstantSpeedPropagationDelayModel");
    channelHelper.AddPropagationLoss(
        "ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(2.7),
        "ReferenceDistance", DoubleValue(1.0));
    Ptr<YansWifiChannel> channel = channelHelper.Create();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Callback<void, Ptr<Socket>> rxCallback;

    ItsStationNetworkConfig networkConfig{};
    networkConfig.initialPosition = Vector(0.0, 0.0, 0.0);
    networkConfig.phyAuditRole = PhyAuditRole::Vru;
    networkConfig.mobilityInstaller = [](Ptr<Node> node)
    {
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(node);
    };

    const ItsStationNetworkResources resources =
        ItsStationNetworkBuilder::Build(
            channel, ipv4, rxCallback, networkConfig);

    {
        vanetza::ManualRuntime runtime(itsStartTime);
        vanetza::geonet::MIB mib;
        ConfigureMib(mib);

        vanetza::geonet::Router router(runtime, mib);
        router.set_address(mib.itsGnLocalGnAddr);
        ConfigurePosition(router, runtime);

        Ptr<Socket> accessSocket =
            accessSocketAvailable ? resources.socket : nullptr;
        Ns3AccessAdapter access(
            resources.node, accessSocket, mib, router);
        Ns3DccAdapter dcc(
            access,
            runtime,
            itsStartTime,
            "vam-real-dcc-access-test",
            resources.node->GetId(),
            9018,
            "vru");
        router.set_access_interface(&dcc);
        VanetzaBtpAdapter btp(router, mib);

        auto context = std::make_shared<PacketContext>();
        context->packetId = packetId;
        context->senderNodeId = resources.node->GetId();
        context->senderStationId = 9018;
        context->simulationStep = 1;

        auto completion = std::make_shared<TxCompletion>(
            [&observation](const TxCompletionResult& result)
            {
                ++observation.completionCalls;
                observation.completionResult = result;
            });

        const std::vector<uint8_t> payload{
            0x80, 0x01, 0x02, 0x7f, 0xa5, 0x5a, 0xfe, 0x11
        };

        VamTransportGateway gateway(
            [&](PacketContext& receivedContext,
                const BtpTxRequest& request,
                const std::vector<uint8_t>& receivedPayload,
                CompletionPtr receivedCompletion)
            {
                observation.callbackContextMatches =
                    &receivedContext == context.get();
                if (!observation.callbackContextMatches ||
                    receivedCompletion != completion)
                {
                    return false;
                }

                TxBridgeContext::Push(context, receivedCompletion);
                const bool routerAccepted = btp.SendBtpB(
                    receivedContext, request, receivedPayload);

                if (!routerAccepted)
                {
                    TxBridgeBinding rejected =
                        TxBridgeContext::TakeIfMatches(
                            context, receivedCompletion);
                    if (rejected.context)
                    {
                        rejected.context->MarkFailure();
                        rejected.context->reason =
                            "vam_btp_router_rejected";
                    }
                    if (rejected.completion)
                    {
                        rejected.completion->Complete({
                            TxCompletionStatus::Failure,
                            Simulator::Now().GetMicroSeconds(),
                            "vam_btp_router_rejected"
                        });
                    }
                    return false;
                }

                TxBridgeBinding orphaned =
                    TxBridgeContext::TakeIfMatches(
                        context, receivedCompletion);
                if (orphaned.context || orphaned.completion)
                {
                    if (orphaned.context)
                    {
                        orphaned.context->MarkFailure();
                        orphaned.context->reason =
                            "router_accepted_without_dcc_handoff";
                    }
                    if (orphaned.completion)
                    {
                        orphaned.completion->Complete({
                            TxCompletionStatus::Failure,
                            Simulator::Now().GetMicroSeconds(),
                            "router_accepted_without_dcc_handoff"
                        });
                    }
                    return false;
                }

                observation.bridgeConsumed = true;
                dcc.SynchronizeRuntimeScheduler();
                return true;
            });

        observation.context = context;
        observation.completion = completion;

        // Keep referenced objects alive until Simulator::Run() returns.
        const auto submit = [&]()
        {
            context->simulationTime = Simulator::Now().GetSeconds();
            context->timestamp = static_cast<uint64_t>(
                Simulator::Now().GetMicroSeconds());

            // Match the RSU production order: clock first, position second.
            dcc.SynchronizeRuntimeForTransmit();
            ConfigurePosition(router, runtime);

            observation.submitAccepted = gateway.Submit(
                *context, payload, 7, completion);

            const TxBridgeBinding current =
                TxBridgeContext::CurrentBinding();

            observation.bridgeEmptyAfterSubmit =
                !current.context && !current.completion;
        };

        if (submitDelay.GetMicroSeconds() == 0)
        {
            submit();
        }
        else
        {
            Simulator::Schedule(submitDelay, submit);
        }

        // The first case exercises a real UDP SendTo and TX_COMPLETE. The
        // second uses a valid Wi-Fi PHY but no Access socket, forcing the
        // production Access adapter's synchronous socket_unavailable path.
        Simulator::Stop(Seconds(1.5));
        Simulator::Run();
    }
    }

    // Network resources and adapters are released before destroying ns-3.
    // A still-pending DCC registry entry resolves during adapter teardown and
    // remains observable through the completion callback above.
    Simulator::Destroy();
    TxBridgeContext::Pop();
    return observation;
}

} // namespace

int main()
{
    bool passed = true;

    std::cout << "===== VAM REAL DCC + ACCESS COMPLETION =====\n";
    const ScenarioObservation success = RunScenario(
        true,
        638101,
        vanetza::Clock::time_point{},
        Seconds(0));

    passed &= Check(
        success.submitAccepted,
        "REAL_DCC_ACCESS_SUBMISSION_ACCEPTED");
    passed &= Check(
        success.callbackContextMatches,
        "REAL_DCC_ACCESS_ORIGINAL_PACKET_CONTEXT");
    passed &= Check(
        success.bridgeConsumed && success.bridgeEmptyAfterSubmit,
        "REAL_DCC_CONSUMES_TX_BRIDGE_BINDING");
    passed &= Check(
        success.completion && success.completion->IsCompleted(),
        "REAL_ACCESS_TERMINAL_COMPLETION_OCCURRED");
    passed &= Check(
        success.completionCalls == 1,
        "REAL_ACCESS_COMPLETION_CALLBACK_EXACTLY_ONCE");
    passed &= Check(
        success.completionResult.status == TxCompletionStatus::Success &&
            success.completionResult.reason == "TX_COMPLETE",
        "REAL_ACCESS_TX_COMPLETE_SUCCESS_RESULT");
    passed &= Check(
        success.context &&
            success.context->pipelineStage == PipelineStage::TX_COMPLETE &&
            success.context->pipelineResult == PipelineResult::OK &&
            success.context->reason == "SendTo OK",
        "REAL_ACCESS_PACKET_CONTEXT_REACHES_TX_COMPLETE");
    passed &= Check(
        success.context && success.context->hasTxBtpMetadata &&
            success.context->txBtpDestinationPort == 2018 &&
            success.context->txItsAid == 638,
        "REAL_ACCESS_VAM_BTP_METADATA_PRESERVED");

    const ScenarioObservation failure = RunScenario(
        false,
        638102,
        vanetza::Clock::time_point{},
        Seconds(0));
    passed &= Check(
        failure.submitAccepted,
        "ACCESS_FAILURE_OCCURS_AFTER_ROUTER_ACCEPTANCE");
    passed &= Check(
        failure.bridgeConsumed && failure.bridgeEmptyAfterSubmit,
        "ACCESS_FAILURE_BINDING_CONSUMED_BY_REAL_DCC");
    passed &= Check(
        failure.completion && failure.completion->IsCompleted(),
        "ACCESS_SOCKET_FAILURE_TERMINAL_COMPLETION_OCCURRED");
    passed &= Check(
        failure.completionCalls == 1 &&
            failure.completionResult.status == TxCompletionStatus::Failure &&
            failure.completionResult.reason == "socket_unavailable",
        "ACCESS_SOCKET_FAILURE_COMPLETES_EXACTLY_ONCE");
    passed &= Check(
        failure.context &&
            failure.context->pipelineStage == PipelineStage::ACCESS_TX &&
            failure.context->pipelineResult == PipelineResult::FAIL &&
            failure.context->reason == "socket_unavailable",
        "ACCESS_SOCKET_FAILURE_MARKS_PACKET_CONTEXT");


    std::cout
        << "\n===== VAM RSU SHB TIMESTAMP REGRESSION =====\n";

    const ScenarioObservation delayed = RunScenario(
        true,
        638103,
        vanetza::Clock::time_point{
            std::chrono::microseconds{5'000'000}},
        MilliSeconds(25));

    passed &= Check(
        delayed.submitAccepted,
        "DELAYED_RSU_SHB_SUBMISSION_ACCEPTED");

    passed &= Check(
        delayed.bridgeConsumed && delayed.bridgeEmptyAfterSubmit,
        "DELAYED_RSU_SHB_DCC_BRIDGE_CONSUMED");

    passed &= Check(
        delayed.completion && delayed.completion->IsCompleted(),
        "DELAYED_RSU_SHB_COMPLETION_OCCURRED");

    passed &= Check(
        delayed.completionCalls == 1 &&
            delayed.completionResult.status == TxCompletionStatus::Success &&
            delayed.completionResult.reason == "TX_COMPLETE",
        "DELAYED_RSU_SHB_COMPLETION_SUCCESS_EXACTLY_ONCE");

    if (!passed)
    {
        std::cout << "VAM_REAL_DCC_ACCESS=FAIL\n";
        return 1;
    }

    std::cout << "VAM_REAL_DCC_ACCESS=PASS\n";
    return 0;
}
