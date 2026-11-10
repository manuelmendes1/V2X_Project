#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"

namespace ns3
{

bool
SumoVruSubjectRegistry::Upsert(
    const std::string& personId,
    VruProfile profile)
{
    if (personId.empty())
    {
        return false;
    }

    // Updating an existing subject must preserve its independent role state.
    auto [subject, inserted] = m_subjects.try_emplace(personId);
    subject->second.profile = profile;
    return true;
}

bool
SumoVruSubjectRegistry::Remove(const std::string& personId)
{
    if (personId.empty())
    {
        return false;
    }

    return m_subjects.erase(personId) != 0;
}

std::optional<SumoVruSubjectSnapshot>
SumoVruSubjectRegistry::Find(const std::string& personId) const
{
    const auto found = m_subjects.find(personId);
    if (found == m_subjects.end())
    {
        return std::nullopt;
    }

    return SumoVruSubjectSnapshot{
        found->second.profile,
        found->second.roleStateMachine.state()};
}

std::optional<SumoVruSubjectRegistry::TransitionResult>
SumoVruSubjectRegistry::Activate(const std::string& personId)
{
    const auto found = m_subjects.find(personId);
    if (found == m_subjects.end())
    {
        return std::nullopt;
    }

    return found->second.roleStateMachine.activate();
}

std::optional<SumoVruSubjectRegistry::TransitionResult>
SumoVruSubjectRegistry::Deactivate(const std::string& personId)
{
    const auto found = m_subjects.find(personId);
    if (found == m_subjects.end())
    {
        return std::nullopt;
    }

    return found->second.roleStateMachine.deactivate();
}

std::optional<SumoVruSubjectRegistry::TransitionResult>
SumoVruSubjectRegistry::SetVbsState(
    const std::string& personId,
    VbsState state)
{
    const auto found = m_subjects.find(personId);
    if (found == m_subjects.end())
    {
        return std::nullopt;
    }

    return found->second.roleStateMachine.setVbsState(state);
}

std::size_t
SumoVruSubjectRegistry::Size() const noexcept
{
    return m_subjects.size();
}

bool
SumoVruSubjectRegistry::Empty() const noexcept
{
    return m_subjects.empty();
}

void
SumoVruSubjectRegistry::Clear() noexcept
{
    m_subjects.clear();
}

} // namespace ns3
