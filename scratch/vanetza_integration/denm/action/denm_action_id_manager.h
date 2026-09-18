#pragma once

#include <cstdint>
#include <unordered_map>

#include "vanetza_integration/denm/types/denm_input.h"

namespace ns3
{

class DenmActionIdManager
{
public:
    static DenmActionIdManager& Instance();

    // Cria uma nova ação DENM para a estação.
    // O sequenceNumber pertence exclusivamente ao lifecycle DENM.
    DenmActionId StartAction(uint32_t originatingStationId);

    // Verifica se o ActionID representa uma ação atualmente ativa.
    bool IsActive(const DenmActionId& actionId) const;

    // Verifica uma transição sem modificar estado.
    bool CanApply(
        const DenmActionId& actionId,
        DenmManagementAction action) const;

    // Aplica a transição e atualiza o estado interno.
    bool Apply(
        const DenmActionId& actionId,
        DenmManagementAction action);

    // Remove todo o estado do manager.
    void Reset();

private:
    struct ActionIdHash
    {
        std::size_t operator()(
            const DenmActionId& actionId) const noexcept;
    };

    struct ActionIdEqual
    {
        bool operator()(
            const DenmActionId& lhs,
            const DenmActionId& rhs) const noexcept
        {
            return lhs.originatingStationId ==
                       rhs.originatingStationId &&
                   lhs.sequenceNumber ==
                       rhs.sequenceNumber;
        }
    };

    enum class LifecycleState : uint8_t
    {
        ACTIVE = 0
    };

    DenmActionIdManager() = default;

    static uint16_t NextSequenceNumber(uint16_t current);

    // Próximo sequenceNumber por originatingStationId.
    std::unordered_map<uint32_t, uint16_t>
        m_nextSequenceByStation;

    // Ações atualmente abertas.
    std::unordered_map<
        DenmActionId,
        LifecycleState,
        ActionIdHash,
        ActionIdEqual>
        m_activeActions;
};

} // namespace ns3
