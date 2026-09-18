#pragma once

#include <string>

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3
{

struct CamTransmissionRequest
{
    VehicleState* vehicle = nullptr;

    bool shouldSend = false;

    bool firstCam = false;

    std::string reason;
};

}
