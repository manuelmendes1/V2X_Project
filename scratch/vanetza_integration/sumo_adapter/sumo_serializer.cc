#include "sumo_serializer.h"

#include <string>

std::string SumoSerializer::Serialize(const ns3::VehicleState& vehicle){
    return vehicle.identity.vehId + "," + std::to_string(vehicle.mobility.position.x) + "," + std::to_string(vehicle.mobility.position.y) + "," + std::to_string(vehicle.mobility.speed) + "," + std::to_string(vehicle.mobility.heading) + "," + std::to_string(vehicle.mobility.accel);
}
