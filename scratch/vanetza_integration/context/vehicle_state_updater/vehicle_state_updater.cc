#include "vehicle_state_updater.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace ns3{

void VehicleStateUpdater::Apply(VehicleState& state, const VehicleState& source){

    state.mobility = source.mobility;
    // Keep the source time associated with the latest received SUMO sample.
    // A legacy sample without a timestamp deliberately clears this optional.
    state.sumoSimulationTimeUs = source.sumoSimulationTimeUs;
    state.initialized = true;

    // Diagnostic evidence is opt-in; normal runs remain quiet.
    // This log occurs after the persistent VehicleState has been updated.
    if (std::getenv("V2X_GEO_AUDIT") != nullptr)
    {
        std::clog
            << "[VEHICLE_STATE_GEODETIC_APPLIED] veh_id="
            << state.identity.vehId
            << " sumo_sim_time_us=";

        if (state.sumoSimulationTimeUs.has_value())
        {
            std::clog << *state.sumoSimulationTimeUs;
        }
        else
        {
            std::clog << "UNAVAILABLE";
        }

        if (state.mobility.geodeticPosition.has_value())
        {
            const auto& geodetic = *state.mobility.geodeticPosition;
            std::clog
                << std::setprecision(10)
                << " longitude_deg=" << geodetic.longitudeDeg
                << " latitude_deg=" << geodetic.latitudeDeg;
        }
        else
        {
            std::clog << " geodetic=UNAVAILABLE";
        }

        std::clog << '\n';
    }
}

}
