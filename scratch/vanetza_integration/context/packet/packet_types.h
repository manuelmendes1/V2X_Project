#pragma once

#include <cstdint>

namespace ns3 {

    // ============================================================================
    // Payload Type
    // ============================================================================

    enum class PayloadType : uint8_t {

        NONE = 0,

        // CAM
        CAM,
        CAM_SIGNED,

        // DENM
        DENM,
        DENM_SIGNED,

        // ETSI Stack
        BTP,
        GEONET,
        IEEE80211P,

        // Transport
        UDP,

        // Generic
        RAW,

        // SPATEM
        SPATEM
    };

    // ============================================================================
    // Pipeline Stage
    // ============================================================================

    enum class PipelineStage : uint8_t {

        NONE = 0,

        // ------------------------------------------------------------------------
        // TX Pipeline
        // ------------------------------------------------------------------------

        GENERATE,
        ENCODE,
        SIGN,

        BTP_TX,

        GEONET_TX,

        ACCESS_TX,

        SEND,

        TX_COMPLETE,

        // ------------------------------------------------------------------------
        // RX Pipeline
        // ------------------------------------------------------------------------

        RAW_RX,

        ACCESS_RX,

        GEONET_RX,

        BTP_RX,

        VERIFY,

        DECODE,

        REPLAY,

        RECEPTION,

        DELIVER
    };

    // ============================================================================
    // Pipeline Result
    // ============================================================================

    enum class PipelineResult : uint8_t {

        NONE = 0,

        BEGIN,

        OK,

        FAIL,

        DROP
    };

    // ============================================================================
    // String conversion
    // ============================================================================

    const char* ToString(PayloadType type);

    const char* ToString(PipelineStage stage);

    const char* ToString(PipelineResult result);

} // namespace ns3
