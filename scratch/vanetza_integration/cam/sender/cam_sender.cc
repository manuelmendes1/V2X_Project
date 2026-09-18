#include "cam_sender.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"


#include "ns3/assert.h"
#include "ns3/simulator.h"

#include <memory>

namespace ns3 {

    void CamSender::SendRaw(
    const std::shared_ptr<PacketContext>& ctx,
    VehicleState& vehicle,
    const std::vector<uint8_t>& rawPayload,
    const std::string&)
{
    NS_ASSERT(ctx != nullptr);

    PacketContext& context = *ctx;

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

        return;
    }

    //----------------------------------------------------------------------
    // Só agora o PacketContext da aplicação fica visível ao Access adapter.
    //----------------------------------------------------------------------
    TxBridgeContext::Push(ctx);

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

        TxBridgeContext::Pop();
        return;
    }

    //----------------------------------------------------------------------
    // ACCESS_TX / SEND / TX_COMPLETE acontecem downstream.
    // O Ns3AccessAdapter captura o shared_ptr e mantém o contexto vivo
    // até a conclusão do envio.
    //----------------------------------------------------------------------
}

} // namespace ns3
