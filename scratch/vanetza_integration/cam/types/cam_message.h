
#pragma once

#include <cstdint>

struct CamMessage{

    uint32_t stationId = 0;

    double latitude = 0.0;
    double longitude = 0.0;

    double speed = 0.0;
    double heading = 0.0;
    double acceleration = 0.0;

    // ETSI EN 302 637-2
    uint16_t generationDeltaTime = 0;

    uint64_t timestamp = 0;
};
