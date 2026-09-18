#include "denm_action_id_manager.h"

#include <functional>

namespace ns3
{

DenmActionIdManager&
DenmActionIdManager::Instance()
{
    static DenmActionIdManager instance;

    return instance;
}

std::size_t
DenmActionIdManager::ActionIdHash::operator()(
    const DenmActionId& actionId) const noexcept
{
    const std::size_t stationHash =
        std::hash<uint32_t>{}(actionId.originatingStationId);

    const std::size_t sequenceHash =
        std::hash<uint16_t>{}(actionId.sequenceNumber);

    return stationHash ^
           (sequenceHash + 0x9e3779b9u +
            (stationHash << 6) +
            (stationHash >> 2));
}

uint16_t
DenmActionIdManager::NextSequenceNumber(
    uint16_t current)
{
    // 0 é reservado ao estado não inicializado.
    // Valores gerados: 1..65535.
    if (current == 65535)
    {
        return 1;
    }

    return static_cast<uint16_t>(current + 1);
}

DenmActionId
DenmActionIdManager::StartAction(
    uint32_t originatingStationId)
{
    if (originatingStationId == 0)
    {
        return {};
    }

    auto& next =
        m_nextSequenceByStation[originatingStationId];

    // Existem 65535 valores válidos no espaço reservado.
    for (uint32_t attempt = 0; attempt < 65535; ++attempt)
    {
        const uint16_t candidate =
            NextSequenceNumber(next);

        DenmActionId actionId;
        actionId.originatingStationId =
            originatingStationId;
        actionId.sequenceNumber =
            candidate;

        next = candidate;

        if (m_activeActions.find(actionId) ==
            m_activeActions.end())
        {
            m_activeActions.emplace(
                actionId,
                LifecycleState::ACTIVE);

            return actionId;
        }
    }

    // Espaço de sequenceNumber esgotado para esta estação.
    return {};
}

bool
DenmActionIdManager::IsActive(
    const DenmActionId& actionId) const
{
    return m_activeActions.find(actionId) !=
           m_activeActions.end();
}

bool
DenmActionIdManager::CanApply(
    const DenmActionId& actionId,
    DenmManagementAction action) const
{
    if (!IsActive(actionId))
    {
        return false;
    }

    switch (action)
    {
        case DenmManagementAction::UPDATE:
        case DenmManagementAction::CANCELLATION:
        case DenmManagementAction::NEGATION:
            return true;

        case DenmManagementAction::NEW:
            return false;
    }

    return false;
}

bool
DenmActionIdManager::Apply(
    const DenmActionId& actionId,
    DenmManagementAction action)
{
    if (!CanApply(actionId, action))
    {
        return false;
    }

    switch (action)
    {
        case DenmManagementAction::UPDATE:
            // O mesmo ActionID continua ativo.
            return true;

        case DenmManagementAction::CANCELLATION:
        case DenmManagementAction::NEGATION:
            // Transição terminal.
            m_activeActions.erase(actionId);
            return true;

        case DenmManagementAction::NEW:
            return false;
    }

    return false;
}

void
DenmActionIdManager::Reset()
{
    m_nextSequenceByStation.clear();
    m_activeActions.clear();
}

} // namespace ns3
