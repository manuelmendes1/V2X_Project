#include "vehicle_mobility_updater.h"

#include "ns3/mobility-module.h"

#include <cmath>

namespace ns3{

void VehicleMobilityUpdater::Apply(VehicleState& state){

    if (!state.node){
        return;
    }

    auto mobility = state.node->GetObject<ConstantVelocityMobilityModel>();

    if (!mobility){
        return;
    }

    mobility->SetPosition(state.mobility.position);

    const double headingRad = (90.0 - state.mobility.heading) * M_PI / 180.0;

    const double speed = state.mobility.speed;

    mobility->SetVelocity(Vector(speed * std::cos(headingRad), speed * std::sin(headingRad),0.0));
}

} // namespace ns3
