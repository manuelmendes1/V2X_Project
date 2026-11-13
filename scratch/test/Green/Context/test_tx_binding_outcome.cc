#include "vanetza_integration/context/tx_bridge/tx_binding_outcome.h"

#include <iostream>
#include <memory>
#include <string>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << name << '\n';
    return condition;
}

}

int main()
{
    bool passed = true;

    // Legacy synchronous DCC failure: context exists, completion does not.
    auto failureContext = std::make_shared<ns3::PacketContext>();
    failureContext->AdvancePipeline(ns3::PipelineStage::BTP_TX);

    ns3::TxBridgeBinding legacyFailure{
        failureContext,
        nullptr
    };

    ns3::ResolveTxBindingFailure(
        legacyFailure,
        1000,
        "dcc_no_pending_runtime_event");

    passed &= Check(
        failureContext->pipelineResult == ns3::PipelineResult::FAIL,
        "LEGACY_DCC_FAILURE_MARKS_CONTEXT_FAIL");

    passed &= Check(
        failureContext->reason == "dcc_no_pending_runtime_event",
        "LEGACY_DCC_FAILURE_PRESERVES_REASON");

    // Legacy DCC lifetime drop: context exists, completion does not.
    auto dropContext = std::make_shared<ns3::PacketContext>();
    dropContext->AdvancePipeline(ns3::PipelineStage::BTP_TX);

    ns3::TxBridgeBinding legacyDrop{
        dropContext,
        nullptr
    };

    ns3::ResolveTxBindingDrop(
        legacyDrop,
        2000,
        "dcc_lifetime_expired",
        "DCC packet lifetime expired");

    passed &= Check(
        dropContext->pipelineResult == ns3::PipelineResult::DROP,
        "LEGACY_DCC_DROP_MARKS_CONTEXT_DROP");

    passed &= Check(
        dropContext->reason == "dcc_lifetime_expired",
        "LEGACY_DCC_DROP_PRESERVES_REASON");

    // Shutdown uses the same drop outcome and must preserve its reason.
    auto shutdownContext = std::make_shared<ns3::PacketContext>();

    ns3::TxBridgeBinding legacyShutdown{
        shutdownContext,
        nullptr
    };

    ns3::ResolveTxBindingDrop(
        legacyShutdown,
        3000,
        "dcc_adapter_shutdown",
        "DCC adapter shutdown before transmission");

    passed &= Check(
        shutdownContext->pipelineResult == ns3::PipelineResult::DROP &&
            shutdownContext->reason == "dcc_adapter_shutdown",
        "LEGACY_DCC_SHUTDOWN_MARKS_CONTEXT_DROP");

    // Preserve the distinct legacy diagnostics for a DCC lifetime drop.
    auto dropCompletionContext =
        std::make_shared<ns3::PacketContext>();
    bool dropCompletionCalled = false;
    ns3::TxCompletionResult dropCompletionResult{};

    auto dropCompletion = std::make_shared<ns3::TxCompletion>(
        [&](const ns3::TxCompletionResult& result)
        {
            dropCompletionCalled = true;
            dropCompletionResult = result;
        });

    ns3::TxBridgeBinding dropCompletionBinding{
        dropCompletionContext,
        dropCompletion
    };

    ns3::ResolveTxBindingDrop(
        dropCompletionBinding,
        2500,
        "dcc_lifetime_expired",
        "DCC packet lifetime expired");

    passed &= Check(
        dropCompletionContext->pipelineResult == ns3::PipelineResult::DROP &&
            dropCompletionContext->reason == "dcc_lifetime_expired",
        "COMPLETION_DCC_DROP_CONTEXT_REASON_PRESERVED");

    passed &= Check(
        dropCompletionCalled &&
            dropCompletionResult.status == ns3::TxCompletionStatus::Failure &&
            dropCompletionResult.completedAtSimUs == 2500 &&
            dropCompletionResult.reason == "DCC packet lifetime expired",
        "COMPLETION_DCC_DROP_CALLBACK_REASON_COMPATIBLE");

    // Shutdown has its own historical callback reason as well.
    auto shutdownCompletionContext =
        std::make_shared<ns3::PacketContext>();
    bool shutdownCompletionCalled = false;
    ns3::TxCompletionResult shutdownCompletionResult{};

    auto shutdownCompletion = std::make_shared<ns3::TxCompletion>(
        [&](const ns3::TxCompletionResult& result)
        {
            shutdownCompletionCalled = true;
            shutdownCompletionResult = result;
        });

    ns3::TxBridgeBinding shutdownCompletionBinding{
        shutdownCompletionContext,
        shutdownCompletion
    };

    ns3::ResolveTxBindingDrop(
        shutdownCompletionBinding,
        3500,
        "dcc_adapter_shutdown",
        "DCC adapter shutdown before transmission");

    passed &= Check(
        shutdownCompletionContext->pipelineResult == ns3::PipelineResult::DROP &&
            shutdownCompletionContext->reason == "dcc_adapter_shutdown",
        "COMPLETION_SHUTDOWN_CONTEXT_REASON_PRESERVED");

    passed &= Check(
        shutdownCompletionCalled &&
            shutdownCompletionResult.status == ns3::TxCompletionStatus::Failure &&
            shutdownCompletionResult.completedAtSimUs == 3500 &&
            shutdownCompletionResult.reason ==
                "DCC adapter shutdown before transmission",
        "COMPLETION_SHUTDOWN_CALLBACK_REASON_COMPATIBLE");

    // A binding with a completion must still notify its owner.
    auto completedContext = std::make_shared<ns3::PacketContext>();
    bool callbackCalled = false;
    ns3::TxCompletionResult callbackResult{};

    auto completion = std::make_shared<ns3::TxCompletion>(
        [&](const ns3::TxCompletionResult& result)
        {
            callbackCalled = true;
            callbackResult = result;
        });

    ns3::TxBridgeBinding completedBinding{
        completedContext,
        completion
    };

    ns3::ResolveTxBindingFailure(
        completedBinding,
        4000,
        "dcc_request_failure");

    passed &= Check(
        completedContext->pipelineResult == ns3::PipelineResult::FAIL &&
            completedContext->reason == "dcc_request_failure",
        "COMPLETION_BINDING_MARKS_CONTEXT_FAIL");

    passed &= Check(
        callbackCalled &&
            callbackResult.status == ns3::TxCompletionStatus::Failure &&
            callbackResult.completedAtSimUs == 4000 &&
            callbackResult.reason == "dcc_request_failure",
        "COMPLETION_BINDING_NOTIFIES_FAILURE");

    if (!passed)
    {
        std::cout << "TX_BINDING_OUTCOME=FAIL\n";
        return 1;
    }

    std::cout << "TX_BINDING_OUTCOME=PASS\n";
    return 0;
}
