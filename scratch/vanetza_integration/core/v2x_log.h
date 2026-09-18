#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "vanetza_integration/context/packet/packet_context.h"

// ============================================================================
// Logging levels
// ============================================================================

enum class V2XLogLevel : uint8_t {

    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR
};

// ============================================================================
// Legacy API
// ============================================================================

void V2XWriteLog(int64_t step, V2XLogLevel level, const std::string& module, const std::string& event, const std::string& vehicleId, uint64_t nodeId, uint64_t stationId, int64_t sequenceId, const std::string& message = "");

// ============================================================================
// PacketContext API
// ============================================================================

void V2XWriteLog(const ns3::PacketContext& context, V2XLogLevel level, const std::string& module, const std::string& event, const std::string& message = "");

// ============================================================================
// Profiling
// ============================================================================

using V2XClock = std::chrono::steady_clock;
using V2XTimePoint = V2XClock::time_point;

V2XTimePoint V2XTimeNow();

int64_t V2XElapsedMicros(const V2XTimePoint& start);
