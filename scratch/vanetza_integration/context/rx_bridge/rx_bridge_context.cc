#include "rx_bridge_context.h"

namespace ns3 {

    thread_local std::vector<PacketContext*>
        RxBridgeContext::s_contexts{};

    void RxBridgeContext::Push(PacketContext* context)
    {
        s_contexts.push_back(context);
    }

    PacketContext* RxBridgeContext::Current()
    {
        if (s_contexts.empty())
        {
            return nullptr;
        }

        return s_contexts.back();
    }

    void RxBridgeContext::Pop() noexcept
    {
        if (!s_contexts.empty())
        {
            s_contexts.pop_back();
        }
    }

    RxBridgeContext::Scope::Scope(PacketContext* context)
        : m_depth(RxBridgeContext::s_contexts.size())
    {
        RxBridgeContext::Push(context);
    }

    RxBridgeContext::Scope::~Scope() noexcept
    {
        // Restore the exact stack depth that existed before this scope.
        // This also removes any nested context left behind during
        // exception unwinding.
        while (RxBridgeContext::s_contexts.size() > m_depth)
        {
            RxBridgeContext::s_contexts.pop_back();
        }
    }

} // namespace ns3
