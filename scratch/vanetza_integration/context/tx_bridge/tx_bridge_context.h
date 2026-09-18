#pragma once

#include <memory>
#include "vanetza_integration/context/packet/packet_context.h"

namespace ns3 {

//==========================================================================
// TxBridgeContext
//
// CORREÇÃO O1 (use-after-free):
//   A versão anterior guardava um ponteiro cru (PacketContext*) para um
//   objeto cujo dono é o CamGenerationContext local da stack em
//   CamScheduler::Run. Como a transmissão real para o canal é diferida
//   (Simulator::ScheduleNow na Ns3AccessAdapter), o lambda corria DEPOIS
//   de o CamGenerationContext — e portanto o PacketContext — já ter sido
//   destruído, lendo/escrevendo memória libertada.
//
//   Agora o bridge guarda um std::shared_ptr<PacketContext>. O lambda da
//   camada Access captura este shared_ptr por valor, mantendo o objeto
//   vivo até TX_COMPLETE, independentemente do tempo de vida do
//   CamGenerationContext que o criou.
//==========================================================================
class TxBridgeContext {
public:
    static void Push(std::shared_ptr<PacketContext> context);
    static std::shared_ptr<PacketContext> Current();
    static void Pop();

private:
    static thread_local std::shared_ptr<PacketContext> m_context;
};

//==========================================================================
// RAII helper
//==========================================================================
class TxBridgeScope {
public:
    explicit TxBridgeScope(std::shared_ptr<PacketContext> context) {
        TxBridgeContext::Push(std::move(context));
    }

    ~TxBridgeScope() {
        TxBridgeContext::Pop();
    }

    TxBridgeScope(const TxBridgeScope&) = delete;
    TxBridgeScope& operator=(const TxBridgeScope&) = delete;
};

} // namespace ns3
