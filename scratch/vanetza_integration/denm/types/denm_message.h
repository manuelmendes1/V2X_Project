#pragma once

#include <cstdint>
#include <optional>

#include "denm_input.h"

struct DenmMessage
{
    DenmManagementAction action = DenmManagementAction::NEW;

    DenmActionId actionId;

    uint64_t detectionTimeItsMs = 0;
    uint64_t referenceTimeItsMs = 0;

    DenmEventPosition eventPosition;

    uint8_t informationQuality = 0;

    DenmCause eventType;

    uint32_t stationType = 0;

    std::optional<uint32_t> validityDurationMs;
    std::optional<uint32_t> transmissionIntervalMs;

    DenmLocationData location;
};
