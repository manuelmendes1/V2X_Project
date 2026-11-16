#include "vanetza_integration/vam/scenario/vam_scenario_subjects.h"

namespace ns3
{

std::vector<VamVruScenarioSubjectConfig>
MakeVamEvaluationScenarioSubjects()
{
    // Synthetic evaluation fixture only; this is not a real-world
    // station identity assignment.
    return {
        VamVruScenarioSubjectConfig{
            "vam-eval-ego-001",
            0x70000001U,
            true,
            VamVruScenarioSubjectConfig::VbsState::ActiveStandalone
        }
    };
}

} // namespace ns3
