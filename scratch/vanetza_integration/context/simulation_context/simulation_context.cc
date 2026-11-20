#include "simulation_context.h"

namespace SimulationContext
{

    uint64_t simulationStep = 0;

    uint64_t packetCounter = 0;

    std::string itsStartTime;
    std::string denmPkiDirectory;
    std::string spatemPkiDirectory;
    std::string cpmPkiDirectory;
    std::optional<ns3::SumoUtmWgs84ProjectionConfig> sumoProjection;

}
