#include "v2x_log.h"

#include <iomanip>
#include <sstream>

#include <ns3/log.h>

namespace {

    const char* ToString(V2XLogLevel level) {
        switch (level) {

            case V2XLogLevel::TRACE: return "TRACE";
            case V2XLogLevel::DEBUG: return "DEBUG";
            case V2XLogLevel::INFO:  return "INFO";
            case V2XLogLevel::WARN:  return "WARN";
            case V2XLogLevel::ERROR: return "ERROR";
        }

        return "UNKNOWN";
    }

} // anonymous namespace

// ============================================================================
// Legacy API
// ============================================================================

void V2XWriteLog(int64_t step, V2XLogLevel level, const std::string& module, const std::string& event, const std::string& vehicleId, uint64_t nodeId, uint64_t stationId, int64_t sequenceId, const std::string& message) {
    std::ostringstream oss;

    oss
        << "[SIM_STEP=" << step << "]"
        << "[VEH=" << (vehicleId.empty() ? "-" : vehicleId) << "]"
        << "[NODE=" << nodeId << "]"
        << "[STATION=" << stationId << "]"
        << "[SEQ=" << sequenceId << "]"
        << "[LVL=" << ToString(level) << "]"
        << "[MOD=" << module << "]"
        << "[EV=" << event << "]";

    if (!message.empty())
        oss << " " << message;

    NS_LOG_UNCOND(oss.str());
}

// ============================================================================
// PacketContext API
// ============================================================================

void V2XWriteLog(const ns3::PacketContext& ctx, V2XLogLevel level, const std::string& module, const std::string& event, const std::string& message) {

    const bool rx = ctx.IsRx();

    const std::string& vehId = rx ? ctx.receiverVehId : ctx.senderVehId;

    const uint32_t nodeId = rx ? ctx.receiverNodeId : ctx.senderNodeId;

    const uint32_t stationId = rx ? ctx.receiverStationId : ctx.senderStationId;

    std::ostringstream oss;

    oss

        << "[SIM_STEP=" << ctx.simulationStep << "]"

        << "[SIM_TIME="
        << std::fixed
        << std::setprecision(6)
        << ctx.simulationTime
        << "s]"

        << "[TIMESTAMP=" << ctx.timestamp << "us]"

        << "[PKT=" << ctx.packetId << "]"

        << "[VPKT=" << ctx.vehiclePacketId << "]"

        << "[SEQ=" << ctx.sequenceId << "]"

        << "[VEH=" << (vehId.empty() ? "-" : vehId) << "]"

        << "[NODE=" << nodeId << "]"

        << "[STATION=" << stationId << "]";

    if (ctx.pipelineStage != ns3::PipelineStage::NONE) {
        oss

            << "[PIPE_STEP="
            << ctx.pipelineStep
            << "/"
            << ctx.pipelineTotal
            << "]"

            << "[STAGE="
            << ns3::ToString(ctx.pipelineStage)
            << "]"

            << "[RESULT="
            << ns3::ToString(ctx.pipelineResult)
            << "]";
    }

    if (ctx.durationUs > 0) {
        oss
            << "[DURATION_US="
            << ctx.durationUs
            << "]";
    }

    if (!ctx.reason.empty()) {
        oss
            << "[REASON="
            << ctx.reason
            << "]";
    }

    oss

        << "[LVL="
        << ToString(level)
        << "]"

        << "[MOD="
        << module
        << "]"

        << "[EV="
        << event
        << "]";

    if (!message.empty())
        oss << " " << message;

    NS_LOG_UNCOND(oss.str());
}

// ============================================================================
// Profiling
// ============================================================================

V2XTimePoint V2XTimeNow() {

    return V2XClock::now();
}

int64_t V2XElapsedMicros(const V2XTimePoint& start) {

    return std::chrono::duration_cast<std::chrono::microseconds>(V2XClock::now() - start).count();
}
