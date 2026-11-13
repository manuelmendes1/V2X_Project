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

    // Updating an existing subject must preserve its independent role
    // state and cluster lifecycle context.
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

    // SUMO REMOVE terminates the source entity. Discard its subject and
    // cluster lifecycle context; this is not a VAM cluster-leave event.
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
        found->second.roleStateMachine.state(),
        found->second.clusterContext
    };
}

std::optional<SumoVruSubjectRegistry::TransitionResult>
SumoVruSubjectRegistry::Activate(const std::string& personId)
{
    const auto found = m_subjects.find(personId);
    if (found == m_subjects.end())
    {
        return std::nullopt;
    }

    if (found->second.clusterContext.phase !=
        vanetza_integration::vam::VruClusterLifecyclePhase::None)
    {
        return TransitionResult::Rejected;
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

    using vanetza_integration::vam::VruClusterLifecyclePhase;

    const auto phase = found->second.clusterContext.phase;

    // A subject cannot disappear from an active cluster lifecycle by
    // directly resetting its role. PassiveLowRisk is not cluster membership.
    if (phase != VruClusterLifecyclePhase::None &&
        phase != VruClusterLifecyclePhase::PassiveLowRisk)
    {
        return TransitionResult::Rejected;
    }

    const auto result = found->second.roleStateMachine.deactivate();

    if (result != TransitionResult::Rejected)
    {
        found->second.clusterContext = {};
    }

    return result;
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

    using vanetza_integration::vam::VruClusterLifecyclePhase;

    if (state != VbsState::ActiveStandalone ||
        found->second.clusterContext.phase !=
            VruClusterLifecyclePhase::None)
    {
        return TransitionResult::Rejected;
    }

    return found->second.roleStateMachine.setVbsState(state);
}

std::optional<SumoVruSubjectRegistry::LifecycleResult>
SumoVruSubjectRegistry::ApplyClusterLifecycle(
    const std::string& personId,
    const LifecycleEvent& event)
{
    const auto found = m_subjects.find(personId);
    if (found == m_subjects.end())
    {
        return std::nullopt;
    }

    auto& subject = found->second;

    auto result =
        vanetza_integration::vam::VruClusterLifecycleStateMachine::Apply(
            subject.roleStateMachine.state(),
            subject.clusterContext,
            event);

    if (result.transition == TransitionResult::Rejected)
    {
        return result;
    }

    const auto& currentRole = subject.roleStateMachine.state();

    // The lifecycle reducer owns VBS transitions, not VRU role activation.
    if (result.nextRole.role != currentRole.role)
    {
        result.transition = TransitionResult::Rejected;
        result.nextRole = currentRole;
        result.nextContext = subject.clusterContext;
        result.operation.reset();
        result.error =
            "cluster lifecycle reducer attempted to change VRU role";
        return result;
    }

    if (result.nextRole.vbs != currentRole.vbs)
    {
        const auto vbsResult =
            subject.roleStateMachine.setVbsState(result.nextRole.vbs);

        if (vbsResult == TransitionResult::Rejected)
        {
            result.transition = TransitionResult::Rejected;
            result.nextRole = currentRole;
            result.nextContext = subject.clusterContext;
            result.operation.reset();
            result.error =
                "role state machine rejected lifecycle VBS transition";
            return result;
        }
    }

    // Context is committed only after the authoritative VBS state accepts
    // the transition. NoChange events may still carry repeated VAM operations.
    subject.clusterContext = result.nextContext;

    return result;
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
