#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
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

struct CompletionRecord
{
    unsigned calls{0};
    TxCompletionResult result{};
};

using CompletionRecordPtr = std::shared_ptr<CompletionRecord>;

struct QueueTeardownObservation
{
    bool firstSubmitAccepted{false};
    bool secondSubmitAccepted{false};
    bool firstBridgeEmptyAfterSubmit{false};
    bool secondBridgeEmptyAfterSubmit{false};
    bool firstPendingAfterFirstSubmit{false};
    bool firstPendingAfterSecondSubmit{false};

    bool firstPendingAfterDccShutdown{false};
    bool secondCompletedAfterDccShutdown{false};
    unsigned firstCallsAfterDccShutdown{0};
    unsigned secondCallsAfterDccShutdown{0};
    TxCompletionResult secondResultAfterDccShutdown{};

    unsigned firstCallsAfterSimulator{0};
    unsigned secondCallsAfterSimulator{0};
    TxCompletionResult firstResultAfterSimulator{};
    TxCompletionResult secondResultAfterSimulator{};

    std::shared_ptr<PacketContext> firstContext;
    std::shared_ptr<PacketContext> secondContext;
    CompletionPtr firstCompletion;
    CompletionPtr secondCompletion;
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

std::shared_ptr<PacketContext> MakePacketContext(
    uint64_t packetId,
    uint32_t nodeId,
    uint32_t stationId,
    uint64_t simulationStep)
{
    auto context = std::make_shared<PacketContext>();
    context->packetId = packetId;
    context->senderNodeId = nodeId;
    context->senderStationId = stationId;
    context->simulationStep = simulationStep;
    context->simulationTime = Simulator::Now().GetSeconds();
    context->timestamp = static_cast<uint64_t>(
        Simulator::Now().GetMicroSeconds());
    return context;
}

CompletionPtr MakeCompletion(const CompletionRecordPtr& record)
{
    return std::make_shared<TxCompletion>(
        [record](const TxCompletionResult& result)
        {
            ++record->calls;
            record->result = result;
        });
}

QueueTeardownObservation RunQueueTeardownScenario()
{
    QueueTeardownObservation observation{};
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
            mobility.SetMobilityModel(
                "ns3::ConstantPositionMobilityModel");
            mobility.Install(node);
        };

        const ItsStationNetworkResources resources =
            ItsStationNetworkBuilder::Build(
                channel, ipv4, rxCallback, networkConfig);

        {
            // Keep Runtime, MIB, Router, Access, and the network resources
            // alive while the DCC adapter is explicitly destroyed and the
            // already-scheduled Access event is allowed to complete.
            vanetza::ManualRuntime runtime(vanetza::Clock::time_point{});
            vanetza::geonet::MIB mib;
            ConfigureMib(mib);

            vanetza::geonet::Router router(runtime, mib);
            router.set_address(mib.itsGnLocalGnAddr);
            ConfigurePosition(router, runtime);

            Ns3AccessAdapter access(
                resources.node, resources.socket, mib, router);

            auto dcc = std::make_unique<Ns3DccAdapter>(
                access,
                runtime,
                vanetza::Clock::time_point{},
                "vam-dcc-queue-teardown-test",
                resources.node->GetId(),
                9018,
                "vru");
            router.set_access_interface(dcc.get());

            VanetzaBtpAdapter btp(router, mib);

            observation.firstContext = MakePacketContext(
                638201, resources.node->GetId(), 9018, 1);
            observation.secondContext = MakePacketContext(
                638202, resources.node->GetId(), 9018, 2);

            auto firstRecord = std::make_shared<CompletionRecord>();
            auto secondRecord = std::make_shared<CompletionRecord>();
            observation.firstCompletion = MakeCompletion(firstRecord);
            observation.secondCompletion = MakeCompletion(secondRecord);

            const std::vector<uint8_t> firstPayload{
                0x80, 0x01, 0x02, 0x7f, 0xa5, 0x5a, 0xfe, 0x11
            };
            const std::vector<uint8_t> secondPayload{
                0x80, 0x02, 0x03, 0x7e, 0xa4, 0x5b, 0xfd, 0x12
            };

            auto makeGateway = [&] (
                const std::shared_ptr<PacketContext>& expectedContext,
                const CompletionPtr& expectedCompletion)
            {
                auto send = [&, expectedContext, expectedCompletion](
                    PacketContext& receivedContext,
                    const BtpTxRequest& request,
                    const std::vector<uint8_t>& payload,
                    CompletionPtr receivedCompletion) -> bool
                {
                    if (&receivedContext != expectedContext.get() ||
                        receivedCompletion != expectedCompletion ||
                        !dcc)
                    {
                        return false;
                    }

                    TxBridgeContext::Push(
                        expectedContext, receivedCompletion);

                    const bool routerAccepted = btp.SendBtpB(
                        receivedContext, request, payload);

                    if (!routerAccepted)
                    {
                        TxBridgeBinding rejected =
                            TxBridgeContext::TakeIfMatches(
                                expectedContext, receivedCompletion);
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

                    // Router acceptance must include handoff to DCC. A
                    // leftover bridge here would be an orphaned transaction.
                    TxBridgeBinding orphaned =
                        TxBridgeContext::TakeIfMatches(
                            expectedContext, receivedCompletion);
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

                    dcc->SynchronizeRuntimeScheduler();
                    return true;
                };

                return std::make_unique<VamTransportGateway>(
                    std::move(send));
            };

            auto firstGateway = makeGateway(
                observation.firstContext,
                observation.firstCompletion);
            auto secondGateway = makeGateway(
                observation.secondContext,
                observation.secondCompletion);

            observation.firstSubmitAccepted = firstGateway->Submit(
                *observation.firstContext,
                firstPayload,
                7,
                observation.firstCompletion);

            const TxBridgeBinding firstCurrent =
                TxBridgeContext::CurrentBinding();
            observation.firstBridgeEmptyAfterSubmit =
                !firstCurrent.context && !firstCurrent.completion;
            observation.firstPendingAfterFirstSubmit =
                !observation.firstCompletion->IsCompleted();

            // Deliberately do not advance ManualRuntime or Simulator between
            // these submissions. The first request should consume the initial
            // DCC transmit opportunity; the second should encounter the same
            // access-category interval and remain queued.
            observation.secondSubmitAccepted = secondGateway->Submit(
                *observation.secondContext,
                secondPayload,
                7,
                observation.secondCompletion);

            const TxBridgeBinding secondCurrent =
                TxBridgeContext::CurrentBinding();
            observation.secondBridgeEmptyAfterSubmit =
                !secondCurrent.context && !secondCurrent.completion;
            observation.firstPendingAfterSecondSubmit =
                !observation.firstCompletion->IsCompleted();

            // Detach Router before destroying the object referenced by its
            // access interface. The Access callback for packet one has already
            // captured its own shared resources and must survive this teardown.
            router.set_access_interface(nullptr);
            dcc.reset();

            observation.firstPendingAfterDccShutdown =
                !observation.firstCompletion->IsCompleted();
            observation.secondCompletedAfterDccShutdown =
                observation.secondCompletion->IsCompleted();
            observation.firstCallsAfterDccShutdown = firstRecord->calls;
            observation.secondCallsAfterDccShutdown = secondRecord->calls;
            observation.secondResultAfterDccShutdown = secondRecord->result;

            // Packet one was handed to Access and has a ScheduleNow event.
            // Packet two must already have been failed by the DCC destructor.
            Simulator::Stop(Seconds(1.5));
            Simulator::Run();

            observation.firstCallsAfterSimulator = firstRecord->calls;
            observation.secondCallsAfterSimulator = secondRecord->calls;
            observation.firstResultAfterSimulator = firstRecord->result;
            observation.secondResultAfterSimulator = secondRecord->result;
        }
    }

    Simulator::Destroy();
    TxBridgeContext::Pop();
    return observation;
}

} // namespace

int main()
{
    bool passed = true;

    std::cout << "===== VAM DCC QUEUED PACKET TEARDOWN =====\n";
    const QueueTeardownObservation result =
        RunQueueTeardownScenario();

    passed &= Check(
        result.firstSubmitAccepted && result.secondSubmitAccepted,
        "DCC_QUEUE_BOTH_GATEWAY_SUBMISSIONS_ACCEPTED");
    passed &= Check(
        result.firstBridgeEmptyAfterSubmit &&
            result.secondBridgeEmptyAfterSubmit,
        "DCC_QUEUE_TX_BRIDGE_CONSUMED_FOR_BOTH_PACKETS");
    passed &= Check(
        result.firstPendingAfterFirstSubmit &&
            result.firstPendingAfterSecondSubmit,
        "DCC_QUEUE_FIRST_ACCESS_COMPLETION_PENDING_BEFORE_SIMULATOR");
    passed &= Check(
        result.firstPendingAfterDccShutdown,
        "DCC_QUEUE_FIRST_ACCESS_COMPLETION_SURVIVES_DCC_TEARDOWN");
    passed &= Check(
        result.secondCompletedAfterDccShutdown,
        "DCC_QUEUE_SECOND_COMPLETION_RESOLVED_DURING_DCC_TEARDOWN");
    passed &= Check(
        result.firstCallsAfterDccShutdown == 0 &&
            result.secondCallsAfterDccShutdown == 1,
        "DCC_QUEUE_SHUTDOWN_COMPLETES_ONLY_QUEUED_PACKET_ONCE");
    passed &= Check(
        result.secondContext &&
            result.secondContext->reason == "dcc_adapter_shutdown",
        "DCC_QUEUE_PACKET_CONTEXT_RECORDS_ADAPTER_SHUTDOWN");
    passed &= Check(
        result.secondResultAfterDccShutdown.status ==
                TxCompletionStatus::Failure &&
            result.secondResultAfterDccShutdown.reason ==
                "DCC adapter shutdown before transmission",
        "DCC_QUEUE_COMPLETION_HAS_SHUTDOWN_FAILURE_REASON");
    passed &= Check(
        result.firstCallsAfterSimulator == 1 &&
            result.firstResultAfterSimulator.status ==
                TxCompletionStatus::Success &&
            result.firstResultAfterSimulator.reason == "TX_COMPLETE",
        "DCC_QUEUE_FIRST_ACCESS_SEND_COMPLETES_AFTER_DCC_TEARDOWN");
    passed &= Check(
        result.firstContext &&
            result.firstContext->pipelineStage == PipelineStage::TX_COMPLETE &&
            result.firstContext->pipelineResult == PipelineResult::OK &&
            result.firstContext->reason == "SendTo OK",
        "DCC_QUEUE_FIRST_PACKET_CONTEXT_REACHES_TX_COMPLETE");
    passed &= Check(
        result.secondCallsAfterSimulator == 1 &&
            result.secondResultAfterSimulator.status ==
                TxCompletionStatus::Failure &&
            result.secondResultAfterSimulator.reason ==
                "DCC adapter shutdown before transmission",
        "DCC_QUEUE_SECOND_COMPLETION_REMAINS_TERMINAL_AFTER_SIMULATOR");

    if (!passed)
    {
        std::cout << "VAM_DCC_QUEUE_TEARDOWN=FAIL\n";
        return 1;
    }

    std::cout << "VAM_DCC_QUEUE_TEARDOWN=PASS\n";
    return 0;
}
