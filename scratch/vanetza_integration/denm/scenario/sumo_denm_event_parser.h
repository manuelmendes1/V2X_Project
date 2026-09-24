#pragma once

#include "denm_scenario_event_source.h"

#include <optional>
#include <string>

namespace ns3
{

class SumoDenmEventParser
{
  public:
    static std::optional<DenmScenarioEvent>
    Parse(const std::string& message);
};

} // namespace ns3
