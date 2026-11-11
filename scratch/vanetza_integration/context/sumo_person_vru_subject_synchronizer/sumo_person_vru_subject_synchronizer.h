#pragma once

#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vru_subject_registry/sumo_vru_subject_registry.h"

namespace ns3
{

// Coordinates the authoritative SUMO Person lifecycle and its derived VRU
// subject state. It owns no mobility data, timestamps, or tombstones.
class SumoVruSubjectSynchronizer
{
public:
    // Applies source validation/order first, then synchronizes the derived
    // subject registry only when the source event has been accepted.
    static bool Apply(
        const SumoPersonDatagram& datagram,
        SumoPersonRegistry& persons,
        SumoVruSubjectRegistry& subjects);

    // Used by SimulationController after its authoritative Person Apply().
    static bool SynchronizeAccepted(
        const SumoPersonDatagram& datagram,
        SumoVruSubjectRegistry& subjects);
};

} // namespace ns3
