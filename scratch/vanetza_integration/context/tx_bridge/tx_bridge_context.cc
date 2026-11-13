#include "tx_bridge_context.h"

#include <utility>

namespace ns3
{

thread_local TxBridgeBinding TxBridgeContext::m_binding{};

void
TxBridgeContext::Push(std::shared_ptr<PacketContext> context)
{
    m_binding.context = std::move(context);
    m_binding.completion.reset();
}

void
TxBridgeContext::Push(
    std::shared_ptr<PacketContext> context,
    std::shared_ptr<TxCompletion> completion)
{
    m_binding.context = std::move(context);
    m_binding.completion = std::move(completion);
}

std::shared_ptr<PacketContext>
TxBridgeContext::Current()
{
    return m_binding.context;
}

TxBridgeBinding
TxBridgeContext::CurrentBinding()
{
    return m_binding;
}

TxBridgeBinding
TxBridgeContext::Take()
{
    TxBridgeBinding binding = std::move(m_binding);
    m_binding = {};
    return binding;
}

TxBridgeBinding
TxBridgeContext::TakeIfMatches(
    const std::shared_ptr<PacketContext>& context,
    const std::shared_ptr<TxCompletion>& completion)
{
    if (!context && !completion)
    {
        return {};
    }

    if (m_binding.context != context ||
        m_binding.completion != completion)
    {
        return {};
    }

    return Take();
}

void
TxBridgeContext::Pop()
{
    m_binding = {};
}

} // namespace ns3
