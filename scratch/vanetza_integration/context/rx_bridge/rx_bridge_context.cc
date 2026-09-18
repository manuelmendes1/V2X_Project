#include "rx_bridge_context.h"

namespace ns3 {

    thread_local PacketContext* RxBridgeContext::s_context = nullptr;

    void RxBridgeContext::Push(PacketContext* context) {
        s_context = context;
    }

    PacketContext* RxBridgeContext::Current() {
        return s_context;
    }

    void RxBridgeContext::Pop() {
        s_context = nullptr;
    }

} // namespace ns3
