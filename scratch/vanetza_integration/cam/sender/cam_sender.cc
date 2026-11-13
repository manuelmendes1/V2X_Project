#include "cam_sender.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/context/tx_bridge/tx_completion.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"


#include "ns3/assert.h"
#include "ns3/simulator.h"

#include <cstdlib>
#include <memory>
#include <string>

namespace ns3 {

    void CamSender::SendRaw(
    const std::shared_ptr<PacketContext>& ctx,
    VehicleState& vehicle,
    const std::vector<uint8_t>& rawPayload,
    const std::string&)
{
    NS_ASSERT(ctx != nullptr);

    PacketContext& context = *ctx;

    // Opt-in terminal TX audit. The normal CAM path remains unchanged.
    std::shared_ptr<TxCompletion> completion;
    const char* completionAudit =
        std::getenv("V2X_TX_COMPLETION_AUDIT");

    if (completionAudit != nullptr &&
        std::string(completionAudit) == "1")
    {
        const std::weak_ptr<PacketContext> weakContext{ctx};

        completion = std::make_shared<TxCompletion>(
            [weakContext](const TxCompletionResult& result)
            {
                const auto completedContext = weakContext.lock();
                if (!completedContext)
                {
                    return;
                }

                const char* status =
                    result.status == TxCompletionStatus::Success
                        ? "SUCCESS"
                        : "FAILURE";

                V2XWriteLog(
                    *completedContext,
                    V2XLogLevel::INFO,
                    "TX",
                    "TX_COMPLETION_CALLBACK",
                    std::string("status=") + status +
                        " completedAtSimUs=" +
                        std::to_string(result.completedAtSimUs) +
                        " reason=" + result.reason);
            });
    }

    //----------------------------------------------------------------------
    // Refresh simulation timing.
    //----------------------------------------------------------------------
    context.simulationStep = SimulationContext::simulationStep;
    context.simulationTime = Simulator::Now().GetSeconds();

    //----------------------------------------------------------------------
    // Preparar relógio e posição ANTES de publicar o PacketContext.
    //
    // IMPORTANTE:
    // PrepareTx() pode executar ManualRuntime::trigger(), que pode gerar
    // transmissões internas do Router. Nesse momento o TxBridgeContext
    // ainda tem de estar vazio.
    //----------------------------------------------------------------------
    if (!vehicle.runtime ||
        !vehicle.runtime->PrepareTx())
    {
        context.MarkFailure();
        context.reason = "tx_prepare_failed";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "TX",
            "TX_PREPARE_FAIL",
            "runtime TX preparation failed");

        if (completion)
        {
            completion->Complete({
                TxCompletionStatus::Failure,
                Simulator::Now().GetMicroSeconds(),
                context.reason
            });
        }

        return;
    }

    //----------------------------------------------------------------------
    // Só agora o PacketContext da aplicação fica visível ao Access adapter.
    //----------------------------------------------------------------------
    if (completion)
    {
        TxBridgeContext::Push(ctx, completion);
    }
    else
    {
        TxBridgeContext::Push(ctx);
    }

    //----------------------------------------------------------------------
    // Step 4/8 : BTP_TX
    //----------------------------------------------------------------------
    context.AdvancePipeline(PipelineStage::BTP_TX);

    bool sent = vehicle.runtime->SendBtpB(
        context,
        2001,
        rawPayload);

    if (!sent)
    {
        context.MarkFailure();
        context.reason = "btp_send_failed";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "BTP",
            "BTP_TX_FAIL",
            "BtpAdapter unavailable or SendBtpB failed");

        if (completion)
        {
            completion->Complete({
                TxCompletionStatus::Failure,
                Simulator::Now().GetMicroSeconds(),
                context.reason
            });
        }

        static_cast<void>(
            TxBridgeContext::TakeIfMatches(ctx, completion));
        return;
    }

    //----------------------------------------------------------------------
    // Router acceptance is not proof of DCC handoff. CAM uses SHB
    // with store-carry-forward disabled, so DCC must consume the binding
    // synchronously if Router processing reaches pass_down().
    TxBridgeBinding orphanedBinding =
        TxBridgeContext::TakeIfMatches(ctx, completion);

    if (orphanedBinding.context || orphanedBinding.completion)
    {
        const std::string reason =
            "router_accepted_without_dcc_handoff";

        if (orphanedBinding.context)
        {
            orphanedBinding.context->MarkFailure();
            orphanedBinding.context->reason = reason;

            V2XWriteLog(
                *orphanedBinding.context,
                V2XLogLevel::ERROR,
                "BTP",
                "BTP_TX_HANDOFF_FAIL",
                "Router accepted request but no packet reached DCC");
        }

        if (orphanedBinding.completion)
        {
            orphanedBinding.completion->Complete({
                TxCompletionStatus::Failure,
                Simulator::Now().GetMicroSeconds(),
                reason
            });
        }
    }

    // ACCESS_TX / SEND / TX_COMPLETE acontecem downstream.
    // O Ns3AccessAdapter captura o shared_ptr e mantém o contexto vivo
    // até a conclusão do envio.
    //----------------------------------------------------------------------
}

} // namespace ns3
