#pragma once

#include <cstdint>
#include <string>

namespace SimulationContext
{
    extern uint64_t simulationStep;

    extern uint64_t packetCounter;

    // Configured ITS absolute start time for the simulation.
    // Empty means no explicit ITS time has been configured.
    extern std::string itsStartTime;
    extern std::string cpmPkiDirectory;
}
