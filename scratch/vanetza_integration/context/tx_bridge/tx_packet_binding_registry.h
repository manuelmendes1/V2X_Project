#pragma once

#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace ns3
{

class TxPacketBindingRegistry final
{
public:
    // Stores a binding without replacing an existing packet association.
    bool Store(
        const void* packetKey,
        TxBridgeBinding binding);

    // Returns a copy of the binding without removing it.
    TxBridgeBinding Find(const void* packetKey) const;

    // Transfers the binding and removes the packet association.
    TxBridgeBinding Take(const void* packetKey);

    // Transfers all pending bindings and clears the registry.
    std::vector<TxBridgeBinding> Drain();

    std::size_t Size() const noexcept;

private:
    // Used by the ns-3 event loop; this registry is not thread-safe.
    std::unordered_map<const void*, TxBridgeBinding> m_bindings;
};

} // namespace ns3
