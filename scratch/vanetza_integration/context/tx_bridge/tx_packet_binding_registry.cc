#include "tx_packet_binding_registry.h"

#include <utility>

namespace ns3
{

bool
TxPacketBindingRegistry::Store(
    const void* packetKey,
    TxBridgeBinding binding)
{
    if (packetKey == nullptr)
    {
        return false;
    }

    return m_bindings.emplace(
        packetKey,
        std::move(binding)).second;
}

TxBridgeBinding
TxPacketBindingRegistry::Find(const void* packetKey) const
{
    if (packetKey == nullptr)
    {
        return {};
    }

    const auto it = m_bindings.find(packetKey);

    if (it == m_bindings.end())
    {
        return {};
    }

    return it->second;
}

TxBridgeBinding
TxPacketBindingRegistry::Take(const void* packetKey)
{
    if (packetKey == nullptr)
    {
        return {};
    }

    const auto it = m_bindings.find(packetKey);

    if (it == m_bindings.end())
    {
        return {};
    }

    TxBridgeBinding binding = std::move(it->second);
    m_bindings.erase(it);

    return binding;
}

std::vector<TxBridgeBinding>
TxPacketBindingRegistry::Drain()
{
    std::vector<TxBridgeBinding> drained;
    drained.reserve(m_bindings.size());

    for (auto& entry : m_bindings)
    {
        drained.push_back(std::move(entry.second));
    }

    m_bindings.clear();
    return drained;
}

std::size_t
TxPacketBindingRegistry::Size() const noexcept
{
    return m_bindings.size();
}

} // namespace ns3
