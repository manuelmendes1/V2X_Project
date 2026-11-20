#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"

namespace SimulationContext
{
    extern uint64_t simulationStep;

    extern uint64_t packetCounter;

    // Configured ITS absolute start time for the simulation.
    // Empty means no explicit ITS time has been configured.
    extern std::string itsStartTime;
    extern std::string cpmPkiDirectory;

    // Optional authoritative projection metadata loaded from SUMO <location>.
    extern std::optional<ns3::SumoUtmWgs84ProjectionConfig> sumoProjection;
}
