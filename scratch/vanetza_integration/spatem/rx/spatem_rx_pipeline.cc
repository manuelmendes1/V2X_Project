#include "spatem_rx_pipeline.h"

#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/spatem/replay/spatem_replay_protection.h"
#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

#include <optional>

namespace ns3
{
namespace
{

constexpr uint32_t kSpatemTimeMarkMaximum = 36001U;
constexpr uint8_t kSpatemConfidenceMaximum = 15U;

bool IsValidTimeMark(const std::optional<uint32_t>& value)
{
    return !value || *value <= kSpatemTimeMarkMaximum;
}

bool IsValidSpatemTiming(const SpatemTiming& timing)
{
    if (timing.minEndTime > kSpatemTimeMarkMaximum ||
        !IsValidTimeMark(timing.startTime) ||
        !IsValidTimeMark(timing.maxEndTime) ||
        !IsValidTimeMark(timing.likelyTime) ||
        !IsValidTimeMark(timing.nextTime))
    {
        return false;
    }

    if (timing.maxEndTime &&
        *timing.maxEndTime < timing.minEndTime)
    {
        return false;
    }

    if (timing.likelyTime &&
        (*timing.likelyTime < timing.minEndTime ||
         (timing.maxEndTime &&
          *timing.likelyTime > *timing.maxEndTime)))
    {
        return false;
    }

    if (timing.confidence &&
        *timing.confidence > kSpatemConfidenceMaximum)
    {
        return false;
    }

    return true;
}

bool HasInvalidSpatemTiming(const SpatemMessage& message)
{
    for (const auto& intersection : message.intersections)
    {
        for (const auto& movement : intersection.movements)
        {
            for (const auto& event : movement.events)
            {
                if (event.timing &&
                    !IsValidSpatemTiming(*event.timing))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace

bool
SpatemRxPipeline::Process(PacketContext& context,
                          const std::vector<uint8_t>& payload,
                          SpatemMessage& message,
                          SpatemReplayContext& replay)
{
    //----------------------------------------------------------------------
    // GEONET_RX
    //----------------------------------------------------------------------
    context.AdvancePipeline(
        PipelineStage::GEONET_RX);

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "SPATEM",
        "SPATEM_GEONET_RX_OK",
        "upper-layer SPATEM indication");

    //----------------------------------------------------------------------
    // BTP_RX
    //----------------------------------------------------------------------
    context.AdvancePipeline(
        PipelineStage::BTP_RX);

    if (context.btpDestinationPort != 2004)
    {
        context.MarkFailure();
        context.reason = "invalid_spatem_btp_port";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "SPATEM",
            "SPATEM_BTP_RX_FAIL",
            "expected port=2004 actual=" +
                std::to_string(context.btpDestinationPort));

        return false;
    }

    context.payload.bytes = payload;
    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "SPATEM",
        "SPATEM_BTP_RX_OK",
        "port=2004 payload=" +
            std::to_string(payload.size()));

    //----------------------------------------------------------------------
    // VERIFY
    //
    // The Vanetza SecurityEntity has already verified the V3 envelope.
    // Require that neutral result before decoding or delivering SPATEM.
    //----------------------------------------------------------------------
    context.AdvancePipeline(
        PipelineStage::VERIFY);

    if (!context.geonet.hasItsAid ||
        context.geonet.itsAid != 137)
    {
        context.MarkFailure();
        context.reason = "invalid_spatem_its_aid";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "SPATEM",
            "SPATEM_VERIFY_FAIL",
            "expected aid=137 actual=" +
                std::to_string(
                    context.geonet.hasItsAid
                        ? context.geonet.itsAid
                        : 0));

        return false;
    }

    if (!context.verificationPerformed ||
        !context.verificationSucceeded ||
        context.verifiedItsAid != 137)
    {
        context.MarkDrop();
        context.reason =
            !context.verificationPerformed
                ? "spatem_v3_verification_missing"
                : (!context.verificationSucceeded
                    ? "spatem_v3_verification_failed"
                    : "spatem_v3_verified_aid_mismatch");

        V2XWriteLog(
            context,
            V2XLogLevel::WARN,
            "SECURITY",
            "SPATEM_CRYPTO_VERIFY_FAIL",
            context.reason);
        return false;
    }

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "SPATEM",
        "SPATEM_VERIFY_OK",
        "aid=137 cryptographic=PASS");

    //----------------------------------------------------------------------
    // DECODE
    //----------------------------------------------------------------------
    context.AdvancePipeline(
        PipelineStage::DECODE);

    const auto decoded =
        VanetzaSpatemCodec::Decode(payload);

    if (!decoded.success)
    {
        context.MarkFailure();
        context.reason = decoded.error;

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "SPATEM",
            "SPATEM_DECODE_FAIL",
            decoded.error);

        return false;
    }

    message = decoded.message;

    // ASN.1/UPER validity does not guarantee semantic timing consistency.
    // Reject invalid timing before identity binding, replay, or delivery.
    if (HasInvalidSpatemTiming(message))
    {
        context.MarkFailure();
        context.reason = "invalid_spatem_timing";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "SPATEM",
            "SPATEM_INVALID_TIMING",
            "one or more timing fields violate SPATEM bounds or ordering");

        return false;
    }

    if (message.stationId == 0 ||
        message.intersections.empty())
    {
        context.MarkFailure();
        context.reason = "invalid_spatem_semantics";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "SPATEM",
            "SPATEM_VALIDATE_FAIL",
            "stationId/intersection list invalid");

        return false;
    }

    //----------------------------------------------------------------------
    // SENDER IDENTITY BINDING
    //
    // The SPATEM stationId must identify the same sender that was resolved
    // by the RX/runtime boundary. This prevents a valid ASN.1 payload from
    // impersonating another station.
    //----------------------------------------------------------------------

    // A pipeline só aceita mensagens cujo emissor tenha sido
    // resolvido pelo boundary RX. stationId == 0 significa identidade
    // ausente, não uma identidade que possa ser ignorada.
    if (context.senderStationId == 0)
    {
        context.MarkDrop();
        context.reason = "spatem_sender_identity_missing";

        V2XWriteLog(
            context,
            V2XLogLevel::WARN,
            "SPATEM",
            "SPATEM_SENDER_IDENTITY_MISSING",
            "RX boundary did not resolve sender station identity");

        return false;
    }

    if (message.stationId != context.senderStationId)
    {
        context.MarkDrop();
        context.reason =
            "spatem_sender_identity_mismatch";

        V2XWriteLog(
            context,
            V2XLogLevel::WARN,
            "SPATEM",
            "SPATEM_SENDER_IDENTITY_MISMATCH",
            "payload_station=" +
                std::to_string(message.stationId) +
            " resolved_sender_station=" +
                std::to_string(context.senderStationId));

        return false;
    }

    context.payload.type = PayloadType::SPATEM;
    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::INFO,
        "SPATEM",
        "SPATEM_DECODE_OK",
        "station=" +
            std::to_string(message.stationId) +
        " intersections=" +
            std::to_string(message.intersections.size()));

    //----------------------------------------------------------------------
    // SELF-ORIGIN
    //
    // PacketContext already carries the authoritative RX endpoint
    // identities. A packet originating from the same node/station must
    // be dropped before replay processing and before application
    // reception/delivery.
    //
    // nodeId 0 is a valid ns-3 node identifier, therefore the test does
    // not use nodeId != 0 as a presence check.
    //----------------------------------------------------------------------

    if (context.senderStationId != 0 &&
        context.senderStationId == context.receiverStationId &&
        context.senderNodeId == context.receiverNodeId &&
        message.stationId == context.senderStationId)
    {
        context.MarkDrop();
        context.reason = "self_reception";

        V2XWriteLog(
            context,
            V2XLogLevel::TRACE,
            "SPATEM",
            "SPATEM_SELF_ORIGIN_DROP",
            "sender and receiver are the same station/node");

        return false;
    }

    //----------------------------------------------------------------------
    // REPLAY
    //----------------------------------------------------------------------
    context.AdvancePipeline(
        PipelineStage::REPLAY);

    if (!SpatemReplayProtection::Process(
            context,
            message,
            replay))
    {
        return false;
    }

    //----------------------------------------------------------------------
    // RECEPTION
    //----------------------------------------------------------------------
    context.AdvancePipeline(
        PipelineStage::RECEPTION);

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::INFO,
        "SPATEM",
        "SPATEM_RECEPTION_OK",
        "station=" +
            std::to_string(message.stationId) +
        " intersection=" +
            std::to_string(
                message.intersections.front().intersectionId));

    //----------------------------------------------------------------------
    // DELIVER
    //----------------------------------------------------------------------
    context.AdvancePipeline(
        PipelineStage::DELIVER);

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::INFO,
        "SPATEM",
        "SPATEM_DELIVER_OK",
        "station=" +
            std::to_string(message.stationId) +
        " intersection=" +
            std::to_string(
                message.intersections.front().intersectionId));

    return true;
}

} // namespace ns3
