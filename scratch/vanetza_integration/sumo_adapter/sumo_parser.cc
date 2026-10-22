#include "sumo_parser.h"

#include "sumo_vehicle_datagram.h"

namespace ns3
{

std::unique_ptr<VehicleState>
SumoParser::Parse(const std::string& message)
{
    const auto datagram = ParseSumoVehicleDatagram(message);
    if (!datagram)
    {
        return nullptr;
    }

    auto vehicle = std::make_unique<VehicleState>();

    vehicle->identity.vehId = datagram->vehId;
    vehicle->mobility.position.x = datagram->x;
    vehicle->mobility.position.y = datagram->y;
    vehicle->mobility.speed = datagram->speed;
    vehicle->mobility.heading = datagram->heading;
    vehicle->mobility.accel = datagram->accel;

    // Preserve WGS84 separately from SUMO's projected x/y coordinates.
    // The pair is accepted only when both coordinates are available.
    if (datagram->longitudeDeg.has_value() &&
        datagram->latitudeDeg.has_value())
    {
        vehicle->mobility.geodeticPosition = VehicleGeodeticPosition{
            *datagram->longitudeDeg,
            *datagram->latitudeDeg
        };
    }

    vehicle->sumoSimulationTimeUs = datagram->simulationTimeUs;
    vehicle->initialized = true;

    return vehicle;
}

} // namespace ns3
