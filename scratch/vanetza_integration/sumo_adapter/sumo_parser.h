#pragma once

#include <memory>
#include <string>

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3 {

class SumoParser {
public:

    // =====================================================
    // Converte uma mensagem proveniente do SUMO
    // num VehicleState.
    // =====================================================

    static std::unique_ptr<VehicleState> Parse(const std::string& message);
};

} // namespace ns3
