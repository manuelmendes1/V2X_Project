#include "denm_rx_pipeline.h"

#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/denm/replay/denm_replay_protection.h"
#include "vanetza_integration/rx/rx_context/rx_context.h"
#include "vanetza_integration/wrappers/vanetza/denm/vanetza_denm_codec.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include <ns3/simulator.h>

namespace ns3
{

void
DenmRxPipeline::Process(
    PacketContext& ctx,
    const std::vector<uint8_t>& payload)
{
    VehicleState* sender = nullptr;
    VehicleState* receiver = nullptr;

    //----------------------------------------------------------------------
    // GEONET_RX
    //----------------------------------------------------------------------
    ctx.AdvancePipeline(
        PipelineStage::GEONET_RX);

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "GEONET",
        "GEONET_RX_OK",
        "DENM upper-layer indication");

    //----------------------------------------------------------------------
    // BTP_RX
    //----------------------------------------------------------------------
    ctx.AdvancePipeline(
        PipelineStage::BTP_RX);

    ctx.payload.type =
        PayloadType::DENM;

    ctx.payload.bytes =
        payload;

    ctx.payload.signatureSize =
        0;

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "BTP",
        "BTP_RX_OK",
        "port=" +
            std::to_string(
                ctx.btpDestinationPort) +
        " payload=" +
            std::to_string(
                payload.size()));

    //----------------------------------------------------------------------
    // Endpoint resolution
    //----------------------------------------------------------------------
    if (!RxContext::Resolve(
            ctx,
            sender,
            receiver))
    {
        return;
    }

    //----------------------------------------------------------------------
    // VERIFY
    //
    // DENM/AID 37 has already been processed by the real ETSI ITS
    // Security V3 SecurityEntity before BTP delivery.
    //
    // PacketContext carries that result across the Vanetza/application
    // boundary. No second cryptographic verification is performed here.
    //----------------------------------------------------------------------
    ctx.AdvancePipeline(
        PipelineStage::VERIFY);

    if (!ctx.geonet.hasItsAid ||
        ctx.geonet.itsAid != 37 ||
        ctx.verifiedItsAid != 37 ||
        !ctx.verificationPerformed)
    {
        ctx.MarkFailure();
        ctx.reason =
            "denm_v3_verification_unavailable";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "SECURITY",
            "VERIFY_FAIL",
            "DENM AID 37 reached application layer "
            "without a V3 verification result");

        return;
    }

    if (!ctx.verificationSucceeded)
    {
        ctx.MarkFailure();
        ctx.reason =
            "denm_v3_verification_failed";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "SECURITY",
            "VERIFY_FAIL",
            "DENM V3 cryptographic verification failed "
            "report=" +
                std::to_string(
                    ctx.verificationReport));

        return;
    }

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::INFO,
        "SECURITY",
        "VERIFY_OK",
        "DENM V3 cryptographic verification succeeded "
        "aid=" +
            std::to_string(
                ctx.verifiedItsAid) +
        " report=" +
            std::to_string(
                ctx.verificationReport));

    //----------------------------------------------------------------------
    // DECODE
    //----------------------------------------------------------------------
    ctx.AdvancePipeline(
        PipelineStage::DECODE);

    const auto decoded =
        VanetzaDenmCodec::Decode(payload);

    if (!decoded.success)
    {
        ctx.MarkFailure();
        ctx.reason =
            decoded.error;

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "DENM_DECODE_FAIL",
            decoded.error);

        return;
    }

    const DenmMessage& message =
        decoded.message;

    //----------------------------------------------------------------------
    // Semantic DENM identity validation
    //
    // ActionID.originatingStationId identifies the semantic origin of the
    // DENM. It MUST NOT be compared to ctx.senderStationId because the
    // GeoNetworking packet may have been forwarded by another station.
    //----------------------------------------------------------------------
    if (message.actionId.originatingStationId == 0 ||
        message.actionId.sequenceNumber == 0)
    {
        ctx.MarkFailure();
        ctx.reason =
            "invalid_denm_action_id";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "DENM_VALIDATE_FAIL",
            "invalid DENM ActionID");

        return;
    }

    //----------------------------------------------------------------------
    // Event position validation
    //----------------------------------------------------------------------
    if (!std::isfinite(
            message.eventPosition.latitude) ||
        !std::isfinite(
            message.eventPosition.longitude) ||
        message.eventPosition.latitude < -90.0 ||
        message.eventPosition.latitude > 90.0 ||
        message.eventPosition.longitude < -180.0 ||
        message.eventPosition.longitude > 180.0)
    {
        ctx.MarkFailure();
        ctx.reason =
            "invalid_denm_event_position";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "DENM_VALIDATE_FAIL",
            "invalid event position");

        return;
    }

    //----------------------------------------------------------------------
    // GeoNetworking source identity
    //
    // senderLinkMac and senderMid represent different identities:
    //
    //   senderLinkMac = immediate ns-3/L2 sender
    //   senderMid     = GeoNetworking source MID
    //
    // The source MID is correlated with the DENM ActionID origin.
    //----------------------------------------------------------------------
    VehicleState* gnSource =
        nullptr;

    std::ostringstream sourceMidText;

    if (ctx.geonet.hasSenderMid)
    {
        for (std::size_t i = 0;
             i < ctx.geonet.senderMid.size();
             ++i)
        {
            if (i != 0)
            {
                sourceMidText << ":";
            }

            sourceMidText
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned>(
                       ctx.geonet.senderMid[i]);
        }

        gnSource =
            VehicleRegistry::Instance().GetByMid(
                ctx.geonet.senderMid);
    }

    if (!ctx.geonet.hasSenderMid)
    {
        ctx.MarkFailure();
        ctx.reason =
            "denm_gn_source_mid_missing";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "DENM_VALIDATE_FAIL",
            "GeoNetworking source MID missing");

        return;
    }

    if (!gnSource)
    {
        ctx.MarkFailure();
        ctx.reason =
            "denm_gn_source_mid_unknown";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "DENM_VALIDATE_FAIL",
            "GeoNetworking source MID not registered");

        return;
    }

    if (gnSource->identity.stationId !=
        message.actionId.originatingStationId)
    {
        ctx.MarkFailure();
        ctx.reason =
            "denm_origin_gn_source_mismatch";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "DENM_VALIDATE_FAIL",
            "DENM ActionID origin does not match "
            "GeoNetworking source");

        return;
    }

    V2XWriteLog(
        ctx,
        V2XLogLevel::INFO,
        "DENM",
        "DENM_GN_SOURCE",
        "origin_station=" +
            std::to_string(
                message.actionId.originatingStationId) +
        " source_mid=" +
            sourceMidText.str() +
        " source_vehicle=" +
            gnSource->identity.vehId +
        " source_station=" +
            std::to_string(
                gnSource->identity.stationId) +
        " immediate_sender=" +
            sender->identity.vehId +
        " immediate_sender_station=" +
            std::to_string(
                sender->identity.stationId) +
        " receiver=" +
            receiver->identity.vehId +
        " receiver_station=" +
            std::to_string(
                receiver->identity.stationId));


    //----------------------------------------------------------------------
    // Self-originated DENM filtering
    //
    // The DENM may return to its originating ITS-S after GeoNetworking
    // forwarding. The ActionID/GN source identifies the semantic origin,
    // while sender identifies only the immediate forwarding hop.
    //
    // A DENM originated by this receiver must not be delivered again as a
    // newly received application event.
    //----------------------------------------------------------------------
    if (gnSource->identity.stationId ==
        receiver->identity.stationId)
    {
        ctx.MarkFailure();
        ctx.reason =
            "denm_self_originated";

        V2XWriteLog(
            ctx,
            V2XLogLevel::WARN,
            "DENM",
            "DENM_SELF_ORIGIN_DROP",
            "origin_station=" +
                std::to_string(
                    message.actionId.originatingStationId) +
            " source_vehicle=" +
                gnSource->identity.vehId +
            " receiver=" +
                receiver->identity.vehId +
            " immediate_sender=" +
                sender->identity.vehId);

        return;
    }

    //----------------------------------------------------------------------
    // Decode success
    //----------------------------------------------------------------------
    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::INFO,
        "DENM",
        "DENM_DECODE_OK",
        "origin_station=" +
            std::to_string(
                message.actionId.originatingStationId) +
        " action_seq=" +
            std::to_string(
                message.actionId.sequenceNumber) +
        " action=" +
            std::to_string(
                static_cast<unsigned>(
                    message.action)) +
        " lat=" +
            std::to_string(
                message.eventPosition.latitude) +
        " lon=" +
            std::to_string(
                message.eventPosition.longitude) +
        " cause=" +
            std::to_string(
                message.eventType.causeCode));

    //----------------------------------------------------------------------
    // REPLAY
    //----------------------------------------------------------------------
    ctx.AdvancePipeline(
        PipelineStage::REPLAY);

    if (!DenmReplayProtection::Process(
            ctx,
            message,
            receiver->security.denmReplay))
    {
        return;
    }

    ctx.MarkSuccess();

    //----------------------------------------------------------------------
    // RECEPTION
    //----------------------------------------------------------------------
    ctx.AdvancePipeline(
        PipelineStage::RECEPTION);

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::INFO,
        "DENM",
        "DENM_RECEPTION_OK",
        "sender=" +
            sender->identity.vehId +
        " receiver=" +
            receiver->identity.vehId +
        " origin_station=" +
            std::to_string(
                message.actionId.originatingStationId) +
        " action_seq=" +
            std::to_string(
                message.actionId.sequenceNumber));

    //----------------------------------------------------------------------
    // DELIVER
    //----------------------------------------------------------------------
    ctx.AdvancePipeline(
        PipelineStage::DELIVER);

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::INFO,
        "DENM",
        "DENM_DELIVER_OK",
        "sender=" +
            sender->identity.vehId +
        " sender_station=" +
            std::to_string(
                sender->identity.stationId) +
        " receiver=" +
            receiver->identity.vehId +
        " receiver_station=" +
            std::to_string(
                receiver->identity.stationId) +
        " pkt=" +
            std::to_string(
                ctx.packetId) +
        " action_seq=" +
            std::to_string(
                message.actionId.sequenceNumber) +
        " lat=" +
            std::to_string(
                message.eventPosition.latitude) +
        " lon=" +
            std::to_string(
                message.eventPosition.longitude));
}

} // namespace ns3