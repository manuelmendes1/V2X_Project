#pragma once

#include <cstdint>
#include <optional>
#include <vector>

enum class SpatemMovementPhaseState : uint8_t
{
    UNAVAILABLE = 0,
    DARK = 1,
    STOP_THEN_PROCEED = 2,
    STOP_AND_REMAIN = 3,
    PRE_MOVEMENT = 4,
    PERMISSIVE_MOVEMENT_ALLOWED = 5,
    PROTECTED_MOVEMENT_ALLOWED = 6,
    PERMISSIVE_CLEARANCE = 7,
    PROTECTED_CLEARANCE = 8,
    CAUTION_CONFLICTING_TRAFFIC = 9
};

struct SpatemTiming
{
    std::optional<uint32_t> startTime;
    uint32_t minEndTime = 0;
    std::optional<uint32_t> maxEndTime;
    std::optional<uint32_t> likelyTime;
    std::optional<uint8_t> confidence;
    std::optional<uint32_t> nextTime;
};

struct SpatemMovementEvent
{
    SpatemMovementPhaseState phaseState =
        SpatemMovementPhaseState::UNAVAILABLE;

    std::optional<SpatemTiming> timing;
};

struct SpatemMovement
{
    uint8_t signalGroup = 0;
    std::vector<SpatemMovementEvent> events;
};

struct SpatemIntersection
{
    std::optional<uint16_t> region;
    uint16_t intersectionId = 0;
    uint8_t revision = 0;

    // Semantic representation of the 16-bit IntersectionStatusObject.
    // Bits 14 and 15 remain reserved and must be zero.
    uint16_t statusBits = 0;

    std::optional<uint32_t> minuteOfYear;
    std::optional<uint16_t> dSecond;

    std::vector<SpatemMovement> movements;
};

struct SpatemInput
{
    uint32_t stationId = 0;
    std::vector<SpatemIntersection> intersections;
};
