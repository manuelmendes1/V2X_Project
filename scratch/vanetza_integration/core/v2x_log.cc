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


    std::string FormatGnMid(const ns3::GeoNetRxContext& geonet)
    {
        if (!geonet.hasSenderMid)
        {
            return "-";
        }

        std::ostringstream out;
        out << std::hex << std::setfill('0');

        for (std::size_t i = 0; i < geonet.senderMid.size(); ++i)
        {
            if (i != 0)
            {
                out << ':';
            }

            out << std::setw(2)
                << static_cast<unsigned>(geonet.senderMid[i]);
        }

        return out.str();
    }

    std::string FormatSenderLinkMac(const ns3::GeoNetRxContext& geonet)
    {
        if (!geonet.hasSenderLinkMac)
        {
            return "-";
        }

        std::ostringstream out;
        out << geonet.senderLinkMac;
        return out.str();
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

        << "[STATION=" << stationId << "]"

        // Preserve the legacy STATION tag while making TX/RX endpoint
        // identities independently auditable.
        << "[SENDER_STATION_ID=" << ctx.senderStationId << "]"
        << "[RECEIVER_STATION_ID=" << ctx.receiverStationId << "]"

        // Packet metadata: objects remain application data, not packet IDs.
        << "[PAYLOAD_TYPE=" << ns3::ToString(ctx.payload.type) << "]"
        << "[PAYLOAD_SIZE=" << ctx.payload.Size() << "]"
        << "[BTP_PORT="
        << (ctx.IsTx()
                ? (ctx.hasTxBtpMetadata &&
                           ctx.txBtpDestinationPort != 0
                       ? std::to_string(ctx.txBtpDestinationPort)
                       : std::string("-"))
                : (ctx.btpDestinationPort != 0
                       ? std::to_string(ctx.btpDestinationPort)
                       : std::string("-")))
        << "]"

        // TX request and observed RX metadata are deliberately independent.
        << "[ITS_AID="
        << (ctx.IsTx()
                ? (ctx.hasTxBtpMetadata && ctx.txItsAid != 0
                       ? std::to_string(ctx.txItsAid)
                       : std::string("-"))
                : (ctx.geonet.hasItsAid && ctx.geonet.itsAid != 0
                       ? std::to_string(ctx.geonet.itsAid)
                       : std::string("-")))
        << "]"
        << "[GN_UPPER_PROTOCOL="
        << (ctx.IsTx()
                ? (ctx.hasTxBtpMetadata
                       ? ctx.txGnUpperProtocol
                       : std::string("-"))
                : (!ctx.geonet.upperProtocol.empty() &&
                           ctx.geonet.upperProtocol != "UNKNOWN"
                       ? ctx.geonet.upperProtocol
                       : std::string("-")))
        << "]"
        << "[GN_TRANSPORT="
        << (ctx.IsTx()
                ? (ctx.hasTxBtpMetadata
                       ? ctx.txGnTransportType
                       : std::string("-"))
                : (!ctx.geonet.transportType.empty() &&
                           ctx.geonet.transportType != "UNKNOWN"
                       ? ctx.geonet.transportType
                       : std::string("-")))
        << "]"
        << "[GN_ADDR_MID=" << FormatGnMid(ctx.geonet) << "]"
        << "[SENDER_LINK_MAC=" << FormatSenderLinkMac(ctx.geonet) << "]"

        // Neutral security result exported by the V3 verification boundary.
        << "[VERIFY_PERFORMED="
        << (ctx.verificationPerformed ? 1 : 0) << "]"
        << "[VERIFY_OK="
        << (ctx.verificationSucceeded ? 1 : 0) << "]"
        << "[VERIFY_REPORT=" << ctx.verificationReport << "]"
        << "[VERIFY_AID=" << ctx.verifiedItsAid << "]";

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
