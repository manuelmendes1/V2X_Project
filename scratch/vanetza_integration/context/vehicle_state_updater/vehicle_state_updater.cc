#include "vehicle_state_updater.h"

namespace ns3{

void VehicleStateUpdater::Apply(VehicleState& state, const VehicleState& source){

    state.mobility = source.mobility;
    state.initialized = true;
}

}
