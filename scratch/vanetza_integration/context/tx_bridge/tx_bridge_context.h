#pragma once

#include <memory>
#include <utility>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/tx_bridge/tx_completion.h"

namespace ns3
{

struct TxBridgeBinding
{
    std::shared_ptr<PacketContext> context;
    std::shared_ptr<TxCompletion> completion;
};

class TxBridgeContext
{
public:
    // Legacy API: publishing a context clears any previous completion.
    static void Push(std::shared_ptr<PacketContext> context);

    // Publish the packet context and its terminal completion together.
    static void Push(
        std::shared_ptr<PacketContext> context,
        std::shared_ptr<TxCompletion> completion);

    static std::shared_ptr<PacketContext> Current();

    // Returns a snapshot without consuming the bridge.
    static TxBridgeBinding CurrentBinding();

    // Transfers both references and clears the bridge.
    static TxBridgeBinding Take();

    // Takes the binding only when both references match the expected
    // application transaction. A mismatch leaves the bridge untouched.
    static TxBridgeBinding TakeIfMatches(
        const std::shared_ptr<PacketContext>& context,
        const std::shared_ptr<TxCompletion>& completion);

    // Clears both references.
    static void Pop();

private:
    static thread_local TxBridgeBinding m_binding;
};

class TxBridgeScope
{
public:
    explicit TxBridgeScope(std::shared_ptr<PacketContext> context)
    {
        TxBridgeContext::Push(std::move(context));
    }

    TxBridgeScope(
        std::shared_ptr<PacketContext> context,
        std::shared_ptr<TxCompletion> completion)
    {
        TxBridgeContext::Push(
            std::move(context),
            std::move(completion));
    }

    ~TxBridgeScope()
    {
        TxBridgeContext::Pop();
    }

    TxBridgeScope(const TxBridgeScope&) = delete;
    TxBridgeScope& operator=(const TxBridgeScope&) = delete;
};

} // namespace ns3
