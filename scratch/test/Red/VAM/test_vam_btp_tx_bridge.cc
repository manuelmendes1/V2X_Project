#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_fix.hpp>
#include <vanetza/dcc/data_request.hpp>
#include <vanetza/dcc/interface.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/net/chunk_packet.hpp>
#include <vanetza/units/angle.hpp>
#include <vanetza/units/length.hpp>
#include <vanetza/units/time.hpp>
#include <vanetza/units/velocity.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/context/tx_bridge/tx_binding_outcome.h"
#include "vanetza_integration/context/tx_bridge/tx_completion.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/vam/transport/vam_transport_policy.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"

// Isolated definitions matching the PacketContext transitions used by the
// already-built production BTP adapter object.
namespace ns3
{

void PacketContext::AdvancePipeline(PipelineStage stage)
{
    pipelineStage = stage;
    pipelineTotal = 8;
    pipelineResult = PipelineResult::BEGIN;

    switch (stage)
    {
    case PipelineStage::BTP_TX:
        pipelineStep = 4;
        break;
    case PipelineStage::GEONET_TX:
        pipelineStep = 5;
        break;
    case PipelineStage::ACCESS_TX:
        pipelineStep = 6;
        break;
    case PipelineStage::SEND:
        pipelineStep = 7;
        break;
    case PipelineStage::TX_COMPLETE:
        pipelineStep = 8;
        break;
    default:
        pipelineStep = 0;
        break;
    }
}

void PacketContext::MarkSuccess()
{
    pipelineResult = PipelineResult::OK;
}

void PacketContext::MarkFailure()
{
    pipelineResult = PipelineResult::FAIL;
}

void PacketContext::MarkDrop()
{
    pipelineResult = PipelineResult::DROP;
}

} // namespace ns3

namespace
{

struct CapturedLog
{
    std::string module;
    std::string event;
    ns3::PipelineStage stage;
    ns3::PipelineResult result;
    std::string reason;
};

std::vector<CapturedLog> g_capturedLogs;

} // namespace

// Capture log metadata so the test can verify the GeoNetworking event
// independently of mutations to the live PacketContext.
void V2XWriteLog(
    const ns3::PacketContext& context,
    V2XLogLevel,
    const std::string& module,
    const std::string& event,
    const std::string&)
{
    g_capturedLogs.push_back({
        module,
        event,
        context.pipelineStage,
        context.pipelineResult,
        context.reason
    });
}

namespace
{

unsigned g_failures = 0;

enum class SynchronousMutation
{
    None,
    AccessSuccess,
    Failure,
    Drop
};

void Check(const char* name, bool condition)
{
    if (condition)
    {
        std::cout << "PASS " << name << '\n';
    }
    else
    {
        std::cerr << "FAIL " << name << '\n';
        ++g_failures;
    }
}

class RecordingDcc final : public vanetza::dcc::RequestInterface
{
public:
    explicit RecordingDcc(
        bool consumeBinding,
        SynchronousMutation mutation = SynchronousMutation::None)
        : m_consumeBinding(consumeBinding),
          m_mutation(mutation)
    {
    }

    unsigned calls{0};
    vanetza::dcc::DataRequest lastRequest{};
    std::unique_ptr<vanetza::ChunkPacket> lastPacket;
    ns3::TxBridgeBinding capturedBinding{};

    void request(
        const vanetza::dcc::DataRequest& request,
        std::unique_ptr<vanetza::ChunkPacket> packet) override
    {
        ++calls;
        lastRequest = request;
        lastPacket = std::move(packet);

        // Mirrors the DCC production boundary's binding-consumption contract.
        // The false setting is a negative control for an unclaimed binding.
        if (m_consumeBinding)
        {
            capturedBinding = ns3::TxBridgeContext::Take();
            ApplySynchronousMutation();
        }
    }

private:
    void ApplySynchronousMutation()
    {
        if (!capturedBinding.context)
        {
            return;
        }

        switch (m_mutation)
        {
        case SynchronousMutation::None:
            break;

        case SynchronousMutation::AccessSuccess:
            capturedBinding.context->AdvancePipeline(
                ns3::PipelineStage::ACCESS_TX);
            capturedBinding.context->MarkSuccess();
            capturedBinding.context->reason =
                "downstream_access_success";
            break;

        case SynchronousMutation::Failure:
            ns3::ResolveTxBindingFailure(
                capturedBinding,
                700001,
                "synchronous_dcc_failure");
            break;

        case SynchronousMutation::Drop:
            ns3::ResolveTxBindingDrop(
                capturedBinding,
                700002,
                "synchronous_dcc_drop",
                "synchronous DCC drop");
            break;
        }
    }

    bool m_consumeBinding;
    SynchronousMutation m_mutation;
};

void ConfigureMib(
    vanetza::geonet::MIB& mib,
    unsigned maximumSduSize)
{
    const vanetza::MacAddress localMac{
        0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    vanetza::geonet::Address localAddress(localMac);

    mib.itsGnLocalGnAddr = localAddress;
    mib.itsGnSecurity = false;
    mib.itsGnMaxSduSize = maximumSduSize;
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

std::shared_ptr<ns3::PacketContext> MakeContext(uint64_t packetId)
{
    auto context = std::make_shared<ns3::PacketContext>();
    context->packetId = packetId;
    context->senderStationId = 9001;
    return context;
}

std::shared_ptr<ns3::TxCompletion> MakeCompletion(
    unsigned& callbackCalls,
    ns3::TxCompletionResult& observedResult)
{
    return std::make_shared<ns3::TxCompletion>(
        [&callbackCalls, &observedResult](
            const ns3::TxCompletionResult& result)
        {
            ++callbackCalls;
            observedResult = result;
        });
}

bool RunAcceptedDccHandoff()
{
    using namespace vanetza;
    using namespace vanetza::geonet;

    ManualRuntime runtime;
    MIB mib;
    ConfigureMib(mib, 1400);

    Router router(runtime, mib);
    router.set_address(mib.itsGnLocalGnAddr);
    ConfigurePosition(router, runtime);

    RecordingDcc dcc(true);
    router.set_access_interface(&dcc);
    ns3::VanetzaBtpAdapter adapter(router, mib);

    auto context = MakeContext(63811);
    unsigned completionCalls = 0;
    ns3::TxCompletionResult observedResult{};
    auto completion = MakeCompletion(completionCalls, observedResult);

    const std::vector<uint8_t> payload{
        0x80, 0x01, 0x02, 0x7F, 0xA5, 0x5A, 0xFE, 0x11};
    const auto request = ns3::MakeVamBtpRequest(7);

    ns3::TxBridgeContext::Push(context, completion);
    const bool accepted = adapter.SendBtpB(*context, request, payload);

    Check("REAL_BTP_ROUTER_ACCEPTS_VAM", accepted);
    Check("REAL_ROUTER_CALLS_DCC_ONCE", dcc.calls == 1);
    Check("DCC_RECEIVES_ORIGINAL_PACKET_CONTEXT",
        dcc.capturedBinding.context == context);
    Check("DCC_RECEIVES_ORIGINAL_TX_COMPLETION",
        dcc.capturedBinding.completion == completion);
    Check("DCC_HANDOFF_CONSUMES_THREAD_LOCAL_BRIDGE",
        !ns3::TxBridgeContext::CurrentBinding().context &&
        !ns3::TxBridgeContext::CurrentBinding().completion);
    Check("DCC_HANDOFF_IS_NOT_TERMINAL_COMPLETION",
        !completion->IsCompleted() && completionCalls == 0);
    Check("DCC_RETAINS_PACKET_AND_BINDING",
        static_cast<bool>(dcc.lastPacket) &&
        dcc.capturedBinding.context == context &&
        dcc.capturedBinding.completion == completion);

    const bool firstTerminal = dcc.capturedBinding.completion->Complete({
        ns3::TxCompletionStatus::Success,
        456789,
        "test_downstream_terminal_success"});
    const bool duplicateTerminal = dcc.capturedBinding.completion->Complete({
        ns3::TxCompletionStatus::Failure,
        456790,
        "duplicate_terminal"});

    Check("DOWNSTREAM_TERMINAL_COMPLETION_ACCEPTED", firstTerminal);
    Check("DOWNSTREAM_DUPLICATE_COMPLETION_REJECTED", !duplicateTerminal);
    Check("DOWNSTREAM_COMPLETION_CALLBACK_EXACTLY_ONCE",
        completionCalls == 1 &&
        observedResult.status == ns3::TxCompletionStatus::Success &&
        observedResult.completedAtSimUs == 456789 &&
        observedResult.reason == "test_downstream_terminal_success");

    return accepted && dcc.calls == 1 &&
        dcc.capturedBinding.context == context &&
        dcc.capturedBinding.completion == completion &&
        completionCalls == 1;
}


bool RunSynchronousDownstreamOutcome(
    SynchronousMutation mutation,
    ns3::PipelineStage expectedStage,
    ns3::PipelineResult expectedResult,
    const std::string& expectedReason,
    const char* testPrefix)
{
    using namespace vanetza;
    using namespace vanetza::geonet;

    ManualRuntime runtime;
    MIB mib;
    ConfigureMib(mib, 1400);

    Router router(runtime, mib);
    router.set_address(mib.itsGnLocalGnAddr);
    ConfigurePosition(router, runtime);

    RecordingDcc dcc(true, mutation);
    router.set_access_interface(&dcc);
    ns3::VanetzaBtpAdapter adapter(router, mib);

    auto context = MakeContext(63820);
    const std::vector<uint8_t> payload{
        0x80, 0x01, 0x02, 0x7F, 0xA5, 0x5A, 0xFE, 0x11};
    const auto request = ns3::MakeVamBtpRequest(7);

    g_capturedLogs.clear();
    ns3::TxBridgeContext::Pop();
    ns3::TxBridgeContext::Push(context);

    const bool accepted =
        adapter.SendBtpB(*context, request, payload);

    const std::string prefix =
        std::string(testPrefix) + "_";

    Check((prefix + "ROUTER_ACCEPTED").c_str(), accepted);
    Check((prefix + "DCC_CALLED_ONCE").c_str(), dcc.calls == 1);
    Check((prefix + "BINDING_CONSUMED").c_str(),
        dcc.capturedBinding.context == context &&
        !ns3::TxBridgeContext::CurrentBinding().context);
    Check((prefix + "LIVE_STAGE_PRESERVED").c_str(),
        context->pipelineStage == expectedStage);
    Check((prefix + "LIVE_RESULT_PRESERVED").c_str(),
        context->pipelineResult == expectedResult);
    Check((prefix + "LIVE_REASON_PRESERVED").c_str(),
        context->reason == expectedReason);

    const auto geonetLog = std::find_if(
        g_capturedLogs.rbegin(),
        g_capturedLogs.rend(),
        [](const CapturedLog& record)
        {
            return record.module == "GEONET" &&
                   record.event == "GEONET_TX_OK";
        });

    const bool geonetLogCorrect =
        geonetLog != g_capturedLogs.rend() &&
        geonetLog->stage == ns3::PipelineStage::GEONET_TX &&
        geonetLog->result == ns3::PipelineResult::OK;

    Check((prefix + "GEONET_LOG_HAS_OWN_STAGE_AND_RESULT").c_str(),
        geonetLogCorrect);

    ns3::TxBridgeContext::Pop();

    return accepted &&
        dcc.calls == 1 &&
        context->pipelineStage == expectedStage &&
        context->pipelineResult == expectedResult &&
        context->reason == expectedReason &&
        geonetLogCorrect;
}

bool RunRouterRejectionRetainsCallerBinding()
{
    using namespace vanetza;
    using namespace vanetza::geonet;

    ManualRuntime runtime;
    MIB mib;
    ConfigureMib(mib, 1); // Deliberately force Router max-SDU rejection.

    Router router(runtime, mib);
    router.set_address(mib.itsGnLocalGnAddr);
    ConfigurePosition(router, runtime);

    RecordingDcc dcc(true);
    router.set_access_interface(&dcc);
    ns3::VanetzaBtpAdapter adapter(router, mib);

    auto context = MakeContext(63812);
    unsigned completionCalls = 0;
    ns3::TxCompletionResult observedResult{};
    auto completion = MakeCompletion(completionCalls, observedResult);
    const std::vector<uint8_t> payload{
        0x80, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
    const auto request = ns3::MakeVamBtpRequest(7);

    ns3::TxBridgeContext::Push(context, completion);
    const bool accepted = adapter.SendBtpB(*context, request, payload);

    const auto stillPublished = ns3::TxBridgeContext::CurrentBinding();
    Check("ROUTER_REJECTION_RETURNS_FALSE", !accepted);
    Check("ROUTER_REJECTION_DOES_NOT_CALL_DCC",
        dcc.calls == 0 && !dcc.lastPacket);
    Check("ROUTER_REJECTION_LEAVES_ORIGINAL_BINDING_WITH_CALLER",
        stillPublished.context == context &&
        stillPublished.completion == completion);
    Check("ROUTER_REJECTION_HAS_NO_PREMATURE_COMPLETION",
        !completion->IsCompleted() && completionCalls == 0);

    auto callerBinding = ns3::TxBridgeContext::TakeIfMatches(
        context, completion);
    Check("CALLER_TAKES_EXACT_REJECTED_BINDING",
        callerBinding.context == context &&
        callerBinding.completion == completion);

    const bool terminal = completion->Complete({
        ns3::TxCompletionStatus::Failure,
        500001,
        "router_rejected_before_dcc_handoff"});
    Check("CALLER_RESOLVES_ROUTER_REJECTION", terminal);
    Check("ROUTER_REJECTION_COMPLETION_EXACTLY_ONCE",
        completionCalls == 1 &&
        observedResult.status == ns3::TxCompletionStatus::Failure &&
        observedResult.reason == "router_rejected_before_dcc_handoff");

    return !accepted && dcc.calls == 0 &&
        callerBinding.context == context &&
        callerBinding.completion == completion &&
        completionCalls == 1;
}

bool RunAcceptedButUnclaimedBindingNegativeControl()
{
    using namespace vanetza;
    using namespace vanetza::geonet;

    ManualRuntime runtime;
    MIB mib;
    ConfigureMib(mib, 1400);

    Router router(runtime, mib);
    router.set_address(mib.itsGnLocalGnAddr);
    ConfigurePosition(router, runtime);

    // Negative control: the DCC test double receives the Router request but
    // deliberately does not claim the TxBridge binding.
    RecordingDcc dcc(false);
    router.set_access_interface(&dcc);
    ns3::VanetzaBtpAdapter adapter(router, mib);

    auto context = MakeContext(63813);
    unsigned completionCalls = 0;
    ns3::TxCompletionResult observedResult{};
    auto completion = MakeCompletion(completionCalls, observedResult);
    const std::vector<uint8_t> payload{
        0x80, 0x01, 0x02, 0x7F, 0xA5, 0x5A, 0xFE, 0x11};
    const auto request = ns3::MakeVamBtpRequest(7);

    ns3::TxBridgeContext::Push(context, completion);
    const bool accepted = adapter.SendBtpB(*context, request, payload);

    Check("NEGATIVE_CONTROL_ROUTER_ACCEPTS", accepted);
    Check("NEGATIVE_CONTROL_DCC_REQUEST_OBSERVED", dcc.calls == 1);
    const auto unclaimed = ns3::TxBridgeContext::CurrentBinding();
    Check("UNCLAIMED_BINDING_REMAINS_VISIBLE_TO_CALLER",
        unclaimed.context == context && unclaimed.completion == completion);

    auto orphaned = ns3::TxBridgeContext::TakeIfMatches(context, completion);
    Check("CALLER_DETECTS_UNCLAIMED_BINDING_AFTER_ACCEPTANCE",
        orphaned.context == context && orphaned.completion == completion);

    const bool terminal = completion->Complete({
        ns3::TxCompletionStatus::Failure,
        500002,
        "router_accepted_without_binding_claim"});
    Check("UNCLAIMED_BINDING_CAN_BE_RESOLVED_AS_FAILURE", terminal);
    Check("UNCLAIMED_BINDING_COMPLETION_EXACTLY_ONCE",
        completionCalls == 1 &&
        observedResult.status == ns3::TxCompletionStatus::Failure &&
        observedResult.reason == "router_accepted_without_binding_claim");

    return accepted && dcc.calls == 1 &&
        orphaned.context == context && orphaned.completion == completion &&
        completionCalls == 1;
}

} // namespace

int main()
{
    std::cout << "===== VAM REAL BTP TX BRIDGE HANDOFF =====\n";

    RunAcceptedDccHandoff();
    RunSynchronousDownstreamOutcome(
        SynchronousMutation::AccessSuccess,
        ns3::PipelineStage::ACCESS_TX,
        ns3::PipelineResult::OK,
        "downstream_access_success",
        "SYNC_ACCESS");
    RunSynchronousDownstreamOutcome(
        SynchronousMutation::Failure,
        ns3::PipelineStage::GEONET_TX,
        ns3::PipelineResult::FAIL,
        "synchronous_dcc_failure",
        "SYNC_FAILURE");
    RunSynchronousDownstreamOutcome(
        SynchronousMutation::Drop,
        ns3::PipelineStage::GEONET_TX,
        ns3::PipelineResult::DROP,
        "synchronous_dcc_drop",
        "SYNC_DROP");
    RunRouterRejectionRetainsCallerBinding();
    RunAcceptedButUnclaimedBindingNegativeControl();

    if (g_failures != 0)
    {
        std::cerr << "VAM_BTP_TX_BRIDGE=FAIL failures="
                  << g_failures << '\n';
        return 1;
    }

    std::cout << "VAM_BTP_TX_BRIDGE=PASS\n";
    return 0;
}
