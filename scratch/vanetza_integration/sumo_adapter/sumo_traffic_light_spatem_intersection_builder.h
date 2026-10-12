#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "sumo_traffic_light_parser.h"
#include "sumo_traffic_light_timing_mapper.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

namespace ns3
{

struct SumoSpatemIntersectionConfig
{
    std::string trafficLightId;

    uint16_t intersectionId = 0;
    uint8_t revision = 0;
    uint16_t statusBits = 0;

    std::optional<uint16_t> region;
};

class SumoTrafficLightSpatemIntersectionBuilder
{
  public:
    explicit SumoTrafficLightSpatemIntersectionBuilder(
        const SumoSpatemIntersectionConfig& config);

    bool Update(
        const SumoTrafficLightState& input,
        const SumoSpatemTimeReference& reference,
        std::string& error);

    const SpatemIntersection& GetIntersection() const;

  private:
    SumoSpatemIntersectionConfig m_config;
    std::map<uint8_t, SpatemMovement> m_movements;
    SpatemIntersection m_intersection;
};

} // namespace ns3
