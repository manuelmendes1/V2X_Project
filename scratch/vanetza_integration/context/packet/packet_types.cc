#include "packet_types.h"

namespace ns3 {

    const char* ToString(PayloadType type) {

        switch (type) {

            case PayloadType::NONE:         return "NONE";

            case PayloadType::CAM:          return "CAM";
            case PayloadType::CAM_SIGNED:   return "CAM_SIGNED";

            case PayloadType::DENM:         return "DENM";
            case PayloadType::DENM_SIGNED:  return "DENM_SIGNED";

            case PayloadType::BTP:          return "BTP";
            case PayloadType::GEONET:       return "GEONET";
            case PayloadType::IEEE80211P:   return "IEEE80211P";

            case PayloadType::UDP:          return "UDP";

            case PayloadType::RAW:          return "RAW";
        }

        return "UNKNOWN";
    }

    const char* ToString(PipelineStage stage) {

        switch (stage) {

            case PipelineStage::NONE:          return "NONE";

            case PipelineStage::GENERATE:      return "GENERATE";
            case PipelineStage::ENCODE:        return "ENCODE";
            case PipelineStage::SIGN:          return "SIGN";

            case PipelineStage::BTP_TX:        return "BTP_TX";
            case PipelineStage::GEONET_TX:     return "GEONET_TX";
            case PipelineStage::ACCESS_TX:     return "ACCESS_TX";

            case PipelineStage::SEND:          return "SEND";
            case PipelineStage::TX_COMPLETE:   return "TX_COMPLETE";

            case PipelineStage::RAW_RX:        return "RAW_RX";

            case PipelineStage::ACCESS_RX:     return "ACCESS_RX";
            case PipelineStage::GEONET_RX:     return "GEONET_RX";
            case PipelineStage::BTP_RX:        return "BTP_RX";

            case PipelineStage::VERIFY:        return "VERIFY";
            case PipelineStage::DECODE:        return "DECODE";
            case PipelineStage::REPLAY:        return "REPLAY";
            case PipelineStage::RECEPTION:     return "RECEPTION";
            case PipelineStage::DELIVER:       return "DELIVER";
        }

        return "UNKNOWN";
    }

    const char* ToString(PipelineResult result) {

        switch (result) {

            case PipelineResult::NONE:   return "NONE";
            case PipelineResult::BEGIN:  return "BEGIN";
            case PipelineResult::OK:     return "OK";
            case PipelineResult::FAIL:   return "FAIL";
            case PipelineResult::DROP:   return "DROP";
        }

        return "UNKNOWN";
    }

} // namespace ns3
