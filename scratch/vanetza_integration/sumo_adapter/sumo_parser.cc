#include "sumo_parser.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/core/v2x_log.h"

#include <sstream>

namespace ns3{

std::unique_ptr<VehicleState> SumoParser::Parse(const std::string& message){

    auto vehicle = std::make_unique<VehicleState>();

    std::stringstream ss(message);
    std::string token;

    try{

        if (!std::getline(ss, vehicle->identity.vehId, ','))
            return nullptr;

        if (!std::getline(ss, token, ','))
            return nullptr;
        vehicle->mobility.position.x = std::stod(token);

        if (!std::getline(ss, token, ','))
            return nullptr;
        vehicle->mobility.position.y = std::stod(token);

        if (!std::getline(ss, token, ','))
            return nullptr;
        vehicle->mobility.speed = std::stod(token);

        if (!std::getline(ss, token, ','))
            return nullptr;
        vehicle->mobility.heading = std::stod(token);

        if (!std::getline(ss, token, ','))
            return nullptr;
        vehicle->mobility.accel = std::stod(token);

        vehicle->initialized = true;
    }
    catch (...)
    {
        //V2XWriteLog(SimulationContext::simulationStep, V2XLogLevel::WARN, "SUMO", "PARSE_FAIL", "-", 0, 0, -1, message);

        return nullptr;
    }

    return vehicle;
}

} // namespace ns3
