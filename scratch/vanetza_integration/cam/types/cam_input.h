#pragma once

#include <cstdint>

struct CamInput
{
    uint32_t stationId;

    double latitude;
    double longitude;

    double speed;
    double heading;
    double acceleration;

    // ETSI GenerationDeltaTime (0..65535 ms)
    uint16_t generationDeltaTime = 0;
};
