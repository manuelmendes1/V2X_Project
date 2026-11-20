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
#include "vanetza_integration/vam/transport/vam_transport_policy.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_dcc_adapter.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"

namespace
{

using namespace ns3;
using CompletionPtr = std::shared_ptr<TxCompletion>;

struct TxObservation
{
    bool submitAccepted{false};
    unsigned completionCalls{0};

    TxCompletionResult result{};

    std::shared_ptr<PacketContext> context;
    CompletionPtr completion;
};

bool Check(bool condition, const char* name)
{
    std::cout
        << (condition ? "PASS " : "FAIL ")
        << name
        << '\n';

    return condition;
}

void InstallConstantPosition(Ptr<Node> node)
{
    MobilityHelper mobility;
    mobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel");
    mobility.Install(node);
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

    position.latitude =
        vanetza::units::GeoAngle::from_value(38.7223);

    position.longitude =
        vanetza::units::GeoAngle::from_value(-9.1393);

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

bool SubmitVamPacket(
    VanetzaBtpAdapter& btp,
    Ns3DccAdapter& dcc,
    uint64_t packetId,
    TxObservation& observation)
{
    observation.context =
        std::make_shared<PacketContext>();

    observation.context->packetId = packetId;
    observation.context->senderNodeId = 0;
    observation.context->senderStationId = 9018;
    observation.context->simulationStep = 1;
    observation.context->simulationTime =
        Simulator::Now().GetSeconds();

    observation.context->timestamp =
        static_cast<uint64_t>(
            Simulator::Now().GetMicroSeconds());

    observation.completion =
        std::make_shared<TxCompletion>(
            [&observation](const TxCompletionResult& result)
            {
                ++observation.completionCalls;
                observation.result = result;
            });

    const std::vector<uint8_t> payload{
        0x80, 0x01, 0x02, 0x7f,
        0xa5, 0x5a, 0xfe, 0x11
    };

    const auto context = observation.context;
    const auto expectedCompletion = observation.completion;

    VamTransportGateway gateway(
        [&btp, &dcc, &observation, context, expectedCompletion](
            PacketContext& receivedContext,
            const BtpTxRequest& request,
            const std::vector<uint8_t>& receivedPayload,
            CompletionPtr receivedCompletion)
        {
            if (&receivedContext != context.get() ||
                receivedCompletion != expectedCompletion)
            {
                return false;
            }

            TxBridgeContext::Push(
                context,
                receivedCompletion);

            const bool routerAccepted =
                btp.SendBtpB(
                    receivedContext,
                    request,
                    receivedPayload);

            if (!routerAccepted)
            {
                TxBridgeBinding rejected =
                    TxBridgeContext::TakeIfMatches(
                        context,
                        receivedCompletion);

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

            // Router acceptance is not sufficient. DCC must have consumed
            // the application binding, either immediately or into its queue.
            TxBridgeBinding orphaned =
                TxBridgeContext::TakeIfMatches(
                    context,
                    receivedCompletion);

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

            // Expose any Runtime callbacks created by Router/BTP processing.
            dcc.SynchronizeRuntimeScheduler();

            return true;
        });

    observation.submitAccepted =
        gateway.Submit(
            *observation.context,
            payload,
            7,
            observation.completion);

    return observation.submitAccepted;
}

} // namespace

int main()
{
    bool passed = true;

    std::cout
        << "===== VAM DCC CBR RESCHEDULE REGRESSION =====\n";

    TxObservation first;
    TxObservation second;

    uint32_t generatorSent = 0;
    uint32_t generatorFailed = 0;

    TxBridgeContext::Pop();

    {
        YansWifiChannelHelper channelHelper;

        channelHelper.SetPropagationDelay(
            "ns3::ConstantSpeedPropagationDelayModel");

        channelHelper.AddPropagationLoss(
            "ns3::LogDistancePropagationLossModel",
            "Exponent", DoubleValue(2.7),
            "ReferenceDistance", DoubleValue(1.0));

        Ptr<YansWifiChannel> channel =
            channelHelper.Create();

        Ipv4AddressHelper ipv4;
        ipv4.SetBase(
            "10.0.0.0",
            "255.255.255.0");

        Callback<void, Ptr<Socket>> noRxCallback;

        // Target station: the actual PHY observed by Ns3ChannelLoadProbe.
        ItsStationNetworkConfig targetConfig{};
        targetConfig.initialPosition =
            Vector(0.0, 0.0, 0.0);
        targetConfig.phyAuditRole = PhyAuditRole::Vru;
        targetConfig.mobilityInstaller =
            InstallConstantPosition;

        const ItsStationNetworkResources target =
            ItsStationNetworkBuilder::Build(
                channel,
                ipv4,
                noRxCallback,
                targetConfig);

        // Independent load generator on the same physical channel.
        ItsStationNetworkConfig generatorConfig{};
        generatorConfig.initialPosition =
            Vector(1.0, 0.0, 0.0);
        generatorConfig.phyAuditRole = PhyAuditRole::Rsu;
        generatorConfig.mobilityInstaller =
            InstallConstantPosition;

        const ItsStationNetworkResources generator =
            ItsStationNetworkBuilder::Build(
                channel,
                ipv4,
                noRxCallback,
                generatorConfig);

        NS_ABORT_MSG_IF(
            target.node == nullptr ||
                target.socket == nullptr ||
                generator.node == nullptr ||
                generator.socket == nullptr,
            "DCC CBR test network initialization failed");

        vanetza::ManualRuntime runtime(
            vanetza::Clock::time_point{});

        vanetza::geonet::MIB mib;
        ConfigureMib(mib);

        vanetza::geonet::Router router(
            runtime,
            mib);

        router.set_address(
            mib.itsGnLocalGnAddr);

        ConfigurePosition(
            router,
            runtime);

        // A null Access socket makes Access complete synchronously with
        // socket_unavailable. This records the exact DCC handoff time
        // without adding target transmissions to the measured CBR.
        Ptr<Socket> unavailableSocket;

        Ns3AccessAdapter access(
            target.node,
            unavailableSocket,
            mib,
            router);

        Ns3DccAdapter dcc(
            access,
            runtime,
            vanetza::Clock::time_point{},
            "vam-dcc-cbr-regression",
            target.node->GetId(),
            9018,
            "vru");

        router.set_access_interface(&dcc);

        VanetzaBtpAdapter btp(
            router,
            mib);

        // Offer enough real Wi-Fi traffic to exceed CBR 0.30 at 100 ms.
        // 32 x 1200-byte datagrams, spaced 2.5 ms apart.
        Ptr<Socket> generatorSocket =
            generator.socket;

        generatorSocket->SetAllowBroadcast(true);

        constexpr uint32_t burstCount = 32;
        constexpr uint32_t payloadBytes = 1200;

        for (uint32_t index = 0;
             index < burstCount;
             ++index)
        {
            const Time sendAt =
                MilliSeconds(5) +
                MicroSeconds(
                    static_cast<int64_t>(index) * 2500);

            Simulator::Schedule(
                sendAt,
                [generatorSocket,
                 &generatorSent,
                 &generatorFailed,
                 payloadBytes]()
                {
                    Ptr<Packet> packet =
                        Create<Packet>(payloadBytes);

                    const int sent =
                        generatorSocket->SendTo(
                            packet,
                            0,
                            InetSocketAddress(
                                Ipv4Address("10.0.0.255"),
                                5000));

                    if (sent >= 0)
                    {
                        ++generatorSent;
                    }
                    else
                    {
                        ++generatorFailed;
                    }
                });
        }

        Simulator::Schedule(
            MilliSeconds(20),
            [&]()
            {
                SubmitVamPacket(
                    btp,
                    dcc,
                    638201,
                    first);
            });

        Simulator::Schedule(
            MilliSeconds(80),
            [&]()
            {
                SubmitVamPacket(
                    btp,
                    dcc,
                    638202,
                    second);
            });

        Simulator::Stop(
            MilliSeconds(250));

        Simulator::Run();
    }

    Simulator::Destroy();
    TxBridgeContext::Pop();

    const int64_t firstAccessUs =
        first.completionCalls == 1
            ? first.result.completedAtSimUs
            : -1;

    const int64_t secondAccessUs =
        second.completionCalls == 1
            ? second.result.completedAtSimUs
            : -1;

    std::cout
        << "OBS first_submit_accepted="
        << (first.submitAccepted ? "yes" : "no")
        << '\n';

    std::cout
        << "OBS second_submit_accepted="
        << (second.submitAccepted ? "yes" : "no")
        << '\n';

    std::cout
        << "OBS generator_sent="
        << generatorSent
        << '\n';

    std::cout
        << "OBS generator_failed="
        << generatorFailed
        << '\n';

    std::cout
        << "OBS first_access_us="
        << firstAccessUs
        << '\n';

    std::cout
        << "OBS second_access_us="
        << secondAccessUs
        << '\n';

    std::cout
        << "OBS first_completion_reason="
        << (first.completionCalls == 1
                ? first.result.reason
                : "missing")
        << '\n';

    std::cout
        << "OBS second_completion_reason="
        << (second.completionCalls == 1
                ? second.result.reason
                : "missing")
        << '\n';

    passed &= Check(
        first.submitAccepted &&
            second.submitAccepted,
        "BOTH_VAM_BTP_SUBMISSIONS_ACCEPTED");

    passed &= Check(
        generatorSent == 32 &&
            generatorFailed == 0,
        "ALL_WIFI_LOAD_DATAGRAMS_ACCEPTED");

    passed &= Check(
        first.completionCalls == 1 &&
            first.result.reason == "socket_unavailable",
        "FIRST_PACKET_REACHED_ACCESS_EXACTLY_ONCE");

    passed &= Check(
        second.completionCalls == 1 &&
            second.result.reason == "socket_unavailable",
        "SECOND_PACKET_REACHED_ACCESS_EXACTLY_ONCE");

    passed &= Check(
        firstAccessUs == 20000,
        "FIRST_PACKET_ACCESS_HANDOFF_AT_20MS");

    passed &= Check(
        secondAccessUs >= 0,
        "SECOND_PACKET_ACCESS_HANDOFF_OBSERVED");

    if (!passed)
    {
        std::cout
            << "VAM_DCC_CBR_HARNESS=FAIL\n";
        return 1;
    }

    std::cout
        << "VAM_DCC_CBR_HARNESS=PASS\n";

    return 0;
}
