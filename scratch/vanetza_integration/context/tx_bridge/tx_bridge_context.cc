#include "tx_bridge_context.h"

namespace ns3 {

thread_local std::shared_ptr<PacketContext> TxBridgeContext::m_context = nullptr;

void TxBridgeContext::Push(std::shared_ptr<PacketContext> context) {
    m_context = std::move(context);
}

std::shared_ptr<PacketContext> TxBridgeContext::Current() {
    return m_context;
}

void TxBridgeContext::Pop() {
    m_context.reset();
}

} // namespace ns3
