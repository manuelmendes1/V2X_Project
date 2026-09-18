#pragma once

#include <string>

#include "cam_transmission_request.h"

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3 {

    class CamTransmission {

        public:

            static bool ShouldTransmit(const VehicleState& current, const CamTriggerSnapshot& previous, double now, std::string& reason);

            static void CommitTransmission(VehicleState& vehicle, double now);

            static CamTransmissionRequest PrepareTransmission(VehicleState& vehicle, double now);

            static bool HighRateTestEnabled();

            static double TestSchedulerInterval();

        private:

            static double TestTransmissionPhase(const VehicleState& vehicle);
    };

} // namespace ns3
