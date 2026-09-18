#pragma once

#include <string>

#include "vanetza_integration/context/vehicle_state.h"

class SumoSerializer{

public:

    // =====================================================
    // Serialize VehicleState -> SUMO message
    // =====================================================

    static std::string Serialize(const ns3::VehicleState& vehicle);
};
