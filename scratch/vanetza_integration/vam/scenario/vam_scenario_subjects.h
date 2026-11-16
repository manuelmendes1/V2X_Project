#pragma once

#include <vector>

#include "vanetza_integration/context/simulation_controller/simulation_controller.h"

namespace ns3
{

// Returns the explicitly declared synthetic VAM evaluation subjects.
// The returned identity is a test identity, not a real-world assignment.
std::vector<VamVruScenarioSubjectConfig>
MakeVamEvaluationScenarioSubjects();

} // namespace ns3
