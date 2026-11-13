#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>

#include "vanetza_integration/vam/cluster/vam_cluster_lifecycle_state_machine.h"
#include "vanetza_integration/vru/role/state_machine/vru_role_state_machine.h"

namespace ns3
{

struct SumoVruSubjectSnapshot
{
    vanetza_integration::vru::role::VruProfile profile{
        vanetza_integration::vru::role::VruProfile::Unknown};
    vanetza_integration::vru::role::VruRoleSnapshot roleState{};
    vanetza_integration::vam::VruClusterLifecycleContext clusterContext{};
};

// Maintains explicit VRU role and cluster lifecycle state for active
// SUMO Person subjects. Mobility and source-event ordering remain owned
// by SumoPersonRegistry.
class SumoVruSubjectRegistry
{
public:
    using TransitionResult =
        vanetza_integration::vru::role::VruTransitionResult;
    using VbsState =
        vanetza_integration::vru::role::VbsState;
    using VruProfile =
        vanetza_integration::vru::role::VruProfile;
    using LifecycleEvent =
        vanetza_integration::vam::VruClusterLifecycleEvent;
    using LifecycleResult =
        vanetza_integration::vam::VruClusterLifecycleResult;

    bool Upsert(const std::string& personId, VruProfile profile);

    bool Remove(const std::string& personId);

    std::optional<SumoVruSubjectSnapshot>
    Find(const std::string& personId) const;

    std::optional<TransitionResult>
    Activate(const std::string& personId);

    std::optional<TransitionResult>
    Deactivate(const std::string& personId);

    // Compatibility entry point for initial ActiveStandalone activation.
    // Cluster-related VBS transitions must use ApplyClusterLifecycle().
    std::optional<TransitionResult>
    SetVbsState(const std::string& personId, VbsState state);

    std::optional<LifecycleResult>
    ApplyClusterLifecycle(
        const std::string& personId,
        const LifecycleEvent& event);

    std::size_t Size() const noexcept;

    bool Empty() const noexcept;

    void Clear() noexcept;

private:
    struct Subject
    {
        VruProfile profile{VruProfile::Unknown};
        vanetza_integration::vru::role::VruRoleStateMachine roleStateMachine{};
        vanetza_integration::vam::VruClusterLifecycleContext clusterContext{};
    };

    std::map<std::string, Subject> m_subjects;
};

} // namespace ns3
