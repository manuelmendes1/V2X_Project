#include "packet_context.h"

#include <sstream>

namespace ns3 {

    // ============================================================================
    // Pipeline mapping
    // ============================================================================

    uint16_t PacketContext::GetPipelineStep(PipelineStage stage) {

        switch (stage) {
            // --------------------------------------------------------------------
            // TX
            // --------------------------------------------------------------------

            case PipelineStage::GENERATE:    return 1;
            case PipelineStage::ENCODE:      return 2;
            case PipelineStage::SIGN:        return 3;
            case PipelineStage::BTP_TX:      return 4;
            case PipelineStage::GEONET_TX:   return 5;
            case PipelineStage::ACCESS_TX:   return 6;
            case PipelineStage::SEND:        return 7;
            case PipelineStage::TX_COMPLETE: return 8;

            // --------------------------------------------------------------------
            // RX
            // --------------------------------------------------------------------

            case PipelineStage::RAW_RX:      return 1;
            case PipelineStage::ACCESS_RX:   return 2;
            case PipelineStage::GEONET_RX:   return 3;
            case PipelineStage::BTP_RX:      return 4;
            case PipelineStage::VERIFY:      return 5;
            case PipelineStage::DECODE:      return 6;
            case PipelineStage::REPLAY:      return 7;
            case PipelineStage::RECEPTION:   return 8;
            case PipelineStage::DELIVER:     return 9;

            default:
                return 0;
        }
    }

    // ============================================================================

    uint16_t PacketContext::GetPipelineTotal(PipelineStage stage) {

        switch (stage) {
            // --------------------------------------------------------------------
            // TX
            // --------------------------------------------------------------------

            case PipelineStage::GENERATE:
            case PipelineStage::ENCODE:
            case PipelineStage::SIGN:
            case PipelineStage::BTP_TX:
            case PipelineStage::GEONET_TX:
            case PipelineStage::ACCESS_TX:
            case PipelineStage::SEND:
            case PipelineStage::TX_COMPLETE:
                return 8;

            // --------------------------------------------------------------------
            // RX
            // --------------------------------------------------------------------

            case PipelineStage::RAW_RX:
            case PipelineStage::ACCESS_RX:
            case PipelineStage::GEONET_RX:
            case PipelineStage::BTP_RX:
            case PipelineStage::VERIFY:
            case PipelineStage::DECODE:
            case PipelineStage::REPLAY:
            case PipelineStage::RECEPTION:
            case PipelineStage::DELIVER:
                return 9;

            default:
                return 0;
        }
    }

    // ============================================================================
    // Pipeline control
    // ============================================================================

    void PacketContext::AdvancePipeline(PipelineStage stage) {

        pipelineStage  = stage;
        pipelineStep   = GetPipelineStep(stage);
        pipelineTotal  = GetPipelineTotal(stage);
        pipelineResult = PipelineResult::BEGIN;
    }

    // ============================================================================
    // Pipeline result
    // ============================================================================

    void PacketContext::MarkSuccess() {

        pipelineResult = PipelineResult::OK;
    }

    void PacketContext::MarkFailure() {

        pipelineResult = PipelineResult::FAIL;
    }

    void PacketContext::MarkDrop() {

        pipelineResult = PipelineResult::DROP;
    }

    void PacketContext::MarkResult(PipelineResult result) {

        pipelineResult = result;
    }

    // ============================================================================
    // Execution helpers
    // ============================================================================

    void PacketContext::ClearExecutionState() {

        pipelineStep   = 0;
        pipelineTotal  = 0;
        pipelineStage  = PipelineStage::NONE;
        pipelineResult = PipelineResult::NONE;

        reason.clear();
        durationUs = 0;
    }

    // ============================================================================
    // Context helpers
    // ============================================================================

    bool PacketContext::HasSender() const {

        return !senderVehId.empty() || senderNodeId != 0 || senderStationId != 0;
    }

    bool PacketContext::HasReceiver() const {

        return !receiverVehId.empty() || receiverNodeId != 0 || receiverStationId != 0;
    }

    bool PacketContext::HasPayload() const {

        return !payload.Empty();
    }

    bool PacketContext::IsTx() const {

        switch (pipelineStage) {

            case PipelineStage::GENERATE:
            case PipelineStage::ENCODE:
            case PipelineStage::SIGN:
            case PipelineStage::BTP_TX:
            case PipelineStage::GEONET_TX:
            case PipelineStage::ACCESS_TX:
            case PipelineStage::SEND:
            case PipelineStage::TX_COMPLETE:
                return true;

            default:
                return false;
        }
    }

    bool PacketContext::IsRx() const {

        switch (pipelineStage) {

            case PipelineStage::RAW_RX:
            case PipelineStage::ACCESS_RX:
            case PipelineStage::GEONET_RX:
            case PipelineStage::BTP_RX:
            case PipelineStage::VERIFY:
            case PipelineStage::DECODE:
            case PipelineStage::REPLAY:
            case PipelineStage::RECEPTION:
            case PipelineStage::DELIVER:
                return true;

            default:
                return false;
        }
    }

    // ============================================================================
    // Validation
    // ============================================================================

    bool PacketContext::IsValid() const {

        return packetId != 0 && senderStationId != 0 && pipelineStage != PipelineStage::NONE;
    }

    bool PacketContext::IsEmpty() const {

        return simulationStep == 0 &&
               simulationTime == 0.0 &&
               timestamp == 0 &&
               senderVehId.empty() &&
               senderNodeId == 0 &&
               senderStationId == 0 &&
               receiverVehId.empty() &&
               receiverNodeId == 0 &&
               receiverStationId == 0 &&
               packetId == 0 &&
               vehiclePacketId == 0 &&
               sequenceId == 0 &&
               pipelineStep == 0 &&
               pipelineTotal == 0 &&
               pipelineStage == PipelineStage::NONE &&
               pipelineResult == PipelineResult::NONE &&
               reason.empty() &&
               durationUs == 0 &&
               payload.Empty();
    }

    // ============================================================================
    // Debug
    // ============================================================================

    std::string PacketContext::ToString() const {

        std::ostringstream oss;

        oss << "PKT=" << packetId
            << " VEH_PKT=" << vehiclePacketId
            << " SEQ=" << sequenceId
            << " TIMESTAMP=" << timestamp << "us"
            << " STEP=" << pipelineStep << "/" << pipelineTotal
            << " STAGE=" << ns3::ToString(pipelineStage)
            << " RESULT=" << ns3::ToString(pipelineResult)
            << " SENDER=" << senderVehId
            << " RECEIVER=" << receiverVehId;

        return oss.str();
    }

    // ============================================================================
    // Lifecycle
    // ============================================================================

    void PacketContext::Reset() {

        simulationStep = 0;
        simulationTime = 0.0;
        timestamp = 0;

        senderVehId.clear();
        senderNodeId = 0;
        senderStationId = 0;

        receiverVehId.clear();
        receiverNodeId = 0;
        receiverStationId = 0;

        packetId = 0;
        vehiclePacketId = 0;
        sequenceId = 0;

        pipelineStep = 0;
        pipelineTotal = 0;
        pipelineStage = PipelineStage::NONE;
        pipelineResult = PipelineResult::NONE;

        reason.clear();
        durationUs = 0;

        payload.Clear();

        geonet = GeoNetRxContext{};

        verificationPerformed = false;
        verificationSucceeded = false;
        verificationReport = 0;
        verifiedItsAid = 0;
    }

} // namespace ns3
