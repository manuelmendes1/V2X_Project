#include <iomanip>
#include "vanetza_integration/cpm/rx/cpm_rx_pipeline.h"

#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/cpm/codec/cpm_uper_decoder.h"
#include "vanetza_integration/cpm/transport/cpm_transport_policy.h"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace ns3
{
namespace
{

constexpr std::int64_t kMaximumTimestampItsMs = 4398046511103LL;
constexpr std::int32_t kMaximumSupportedObjectAgeMs = 1500;

bool ValidateCpmSemantics(
    const CpmMessage& message,
    std::string& error)
{
    if (message.station.stationId == 0)
    {
        error = "cpm_station_id_invalid";
        return false;
    }

    if (!std::holds_alternative<CpmRoadSideUnitStationData>(
            message.station.origin))
    {
        error = "cpm_station_origin_outside_supported_rsu_profile";
        return false;
    }

    if (message.referenceTimeItsMs < 0 ||
        message.referenceTimeItsMs > kMaximumTimestampItsMs)
    {
        error = "cpm_reference_time_out_of_range";
        return false;
    }

    const auto& reference = message.referencePosition;

    // The supported RSU application profile requires a usable WGS84
    // horizontal reference position for interpreting relative object data.
    if (!reference.latitudeDeg || !reference.longitudeDeg)
    {
        error = "cpm_reference_position_unavailable";
        return false;
    }

    if (!std::isfinite(*reference.latitudeDeg) ||
        !std::isfinite(*reference.longitudeDeg) ||
        *reference.latitudeDeg < -90.0 ||
        *reference.latitudeDeg > 90.0 ||
        *reference.longitudeDeg < -180.0 ||
        *reference.longitudeDeg > 180.0)
    {
        error = "cpm_reference_position_invalid";
        return false;
    }

    if (message.perceivedObjects.size() > 255 ||
        message.knownObjectCount < message.perceivedObjects.size())
    {
        error = "cpm_object_cardinality_invalid";
        return false;
    }

    std::unordered_set<std::uint16_t> objectIds;
    objectIds.reserve(message.perceivedObjects.size());

    for (const auto& object : message.perceivedObjects)
    {
        if (object.objectId == 0 ||
            !objectIds.insert(object.objectId).second)
        {
            error = "cpm_object_id_missing_or_duplicate";
            return false;
        }

        if (object.measurementDeltaTimeMs < -2048 ||
            object.measurementDeltaTimeMs > 2047)
        {
            error = "cpm_measurement_delta_time_out_of_range";
            return false;
        }

        if (object.objectAgeMs &&
            (*object.objectAgeMs < 0 ||
             *object.objectAgeMs > kMaximumSupportedObjectAgeMs))
        {
            error = "cpm_object_age_out_of_supported_range";
            return false;
        }

        if (!std::isfinite(object.position.xEast.value) ||
            !std::isfinite(object.position.yNorth.value))
        {
            error = "cpm_object_position_non_finite";
            return false;
        }
    }

    error.clear();
    return true;
}

template <typename Objects>
std::string ObjectIds(const Objects& objects)
{
    std::ostringstream out;
    bool first = true;

    for (const auto& object : objects)
    {
        if (!first)
        {
            out << ',';
        }

        out << object.objectId;
        first = false;
    }

    return first ? "-" : out.str();
}

std::string PayloadFingerprint(
    const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t hash = 14695981039346656037ULL;

    for (const std::uint8_t byte : bytes)
    {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string ReferencePositionSummary(
    const CpmReferencePosition& reference)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(7)
        << "reference_latitude_deg=";

    if (reference.latitudeDeg.has_value())
    {
        out << *reference.latitudeDeg;
    }
    else
    {
        out << "unavailable";
    }

    out << " reference_longitude_deg=";
    if (reference.longitudeDeg.has_value())
    {
        out << *reference.longitudeDeg;
    }
    else
    {
        out << "unavailable";
    }

    return out.str();
}

template <typename Objects>
std::string ObjectPositionsEnu(const Objects& objects)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);

    bool first = true;
    for (const auto& object : objects)
    {
        if (!first)
        {
            out << ';';
        }

        out << object.objectId << ':'
            << object.position.xEast.value << ','
            << object.position.yNorth.value;

        first = false;
    }

    return first ? "-" : out.str();
}

template <typename Objects>
std::string ObjectTemporalSummary(const Objects& objects)
{
    std::ostringstream out;
    bool first = true;

    for (const auto& object : objects)
    {
        if (!first)
        {
            out << ';';
        }

        out << object.objectId
            << ":measurement_delta_ms="
            << object.measurementDeltaTimeMs
            << ",object_age_ms=";

        if (object.objectAgeMs.has_value())
        {
            out << *object.objectAgeMs;
        }
        else
        {
            out << "unavailable";
        }

        first = false;
    }

    return first ? "-" : out.str();
}

std::string MessageSummary(const CpmMessage& message)
{
    return "station=" + std::to_string(message.station.stationId) +
        " reference_time_its_ms=" +
            std::to_string(message.referenceTimeItsMs) +
        " known_objects=" + std::to_string(message.knownObjectCount) +
        " included_objects=" +
            std::to_string(message.perceivedObjects.size()) +
        " object_ids=" + ObjectIds(message.perceivedObjects) +
        " " + ReferencePositionSummary(message.referencePosition) +
        " object_positions_enu_m=" +
            ObjectPositionsEnu(message.perceivedObjects) +
        " object_temporal_ms=" +
            ObjectTemporalSummary(message.perceivedObjects);
}

bool Fail(
    PacketContext& context,
    const char* event,
    const std::string& reason,
    PipelineResult result = PipelineResult::FAIL)
{
    context.reason = reason;
    context.MarkResult(result);

    V2XWriteLog(
        context,
        result == PipelineResult::DROP
            ? V2XLogLevel::WARN
            : V2XLogLevel::ERROR,
        "CPM",
        event,
        reason);

    return false;
}

} // namespace

bool
CpmRxPipeline::Process(
    PacketContext& context,
    const std::vector<std::uint8_t>& payload,
    CpmMessage& message,
    std::int64_t receivedAtUnixMs,
    CpmReplayContext& replay,
    CpmReceptionQueue& receptionQueue)
{
    // GEONET_RX: the Router has delivered an upper-layer indication.
    context.AdvancePipeline(PipelineStage::GEONET_RX);
    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_GEONET_RX_OK",
        "GeoNetworking upper-layer indication");

    // BTP_RX: check the application boundary and retain the original bytes.
    context.AdvancePipeline(PipelineStage::BTP_RX);

    if (context.btpDestinationPort != kCpmBtpPort)
    {
        return Fail(
            context,
            "CPM_BTP_RX_FAIL",
            "cpm_invalid_btp_port");
    }

    if (payload.empty())
    {
        return Fail(
            context,
            "CPM_BTP_RX_FAIL",
            "cpm_empty_payload");
    }

    context.payload.bytes = payload;
    context.payload.type = PayloadType::CPM;
    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_BTP_RX_OK",
        "port=" + std::to_string(kCpmBtpPort) +
        " payload=" + std::to_string(payload.size()) +
        " payload_fnv1a64=" + PayloadFingerprint(payload));

    // VERIFY: validate the advertised CPM transport profile. This is not
    // cryptographic signature verification; that remains a separate gate
    // for Etapa 10.10.
    context.AdvancePipeline(PipelineStage::VERIFY);

    if (!context.geonet.hasItsAid ||
        context.geonet.itsAid != kCpmItsAid)
    {
        return Fail(
            context,
            "CPM_PROFILE_CHECK_FAIL",
            "cpm_invalid_its_aid");
    }

    // SECURITY GATE: consume the neutral verification result produced
    // by the Vanetza security layer. This check is not itself a signature
    // verifier. Until the real CPM verifier is wired into RX, fail closed.
    if (!context.verificationPerformed)
    {
        return Fail(
            context,
            "CPM_CRYPTO_VERIFY_FAIL",
            "cpm_cryptographic_verification_missing",
            PipelineResult::DROP);
    }

    if (!context.verificationSucceeded)
    {
        return Fail(
            context,
            "CPM_CRYPTO_VERIFY_FAIL",
            "cpm_cryptographic_verification_failed",
            PipelineResult::DROP);
    }

    if (context.verifiedItsAid != kCpmItsAid)
    {
        return Fail(
            context,
            "CPM_CRYPTO_VERIFY_FAIL",
            "cpm_verified_its_aid_mismatch",
            PipelineResult::DROP);
    }

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_PROFILE_CHECK_OK",
        "BTP profile and successful security verification accepted for AID 639");

    // DECODE: convert ASN.1/UPER into the neutral CpmMessage.
    context.AdvancePipeline(PipelineStage::DECODE);

    const auto decoded =
        CpmUperDecoder::Decode(payload, receivedAtUnixMs);

    if (!decoded.success || !decoded.message)
    {
        return Fail(
            context,
            "CPM_DECODE_FAIL",
            "cpm_decode_failed: " + decoded.error);
    }

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_DECODE_OK",
        MessageSummary(*decoded.message));

    std::string semanticError;

    if (!ValidateCpmSemantics(*decoded.message, semanticError))
    {
        return Fail(
            context,
            "CPM_SEMANTIC_VALIDATION_FAIL",
            semanticError);
    }

    // Bind the semantic origin to the identity resolved by the RX boundary.
    if (context.senderStationId == 0)
    {
        return Fail(
            context,
            "CPM_SENDER_IDENTITY_FAIL",
            "cpm_sender_identity_missing",
            PipelineResult::DROP);
    }

    if (decoded.message->station.stationId != context.senderStationId)
    {
        return Fail(
            context,
            "CPM_SENDER_IDENTITY_FAIL",
            "cpm_sender_identity_mismatch",
            PipelineResult::DROP);
    }

    if (context.senderStationId == context.receiverStationId &&
        context.senderNodeId == context.receiverNodeId)
    {
        return Fail(
            context,
            "CPM_SELF_RECEPTION_DROP",
            "cpm_self_reception",
            PipelineResult::DROP);
    }

    // Reserve application capacity before mutating replay history. ns-3
    // processes this callback synchronously; no other callback can fill this
    // queue between this check and the subsequent push.
    if (!receptionQueue.CanAccept())
    {
        return Fail(
            context,
            "CPM_APPLICATION_QUEUE_FULL",
            "cpm_application_reception_queue_full",
            PipelineResult::DROP);
    }

    // REPLAY: suppress exact duplicate payloads per station within the
    // bounded local window. This is preliminary duplicate protection,
    // not proof of signature-bound replay protection.
    context.AdvancePipeline(PipelineStage::REPLAY);

    if (!replay.AcceptIfNew(
            decoded.message->station.stationId,
            payload))
    {
        return Fail(
            context,
            "CPM_REPLAY_DROP",
            "cpm_exact_payload_replay_detected",
            PipelineResult::DROP);
    }

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "CPM",
        "CPM_REPLAY_OK",
        MessageSummary(*decoded.message));

    // RECEPTION: publish the validated neutral representation to the caller.
    context.AdvancePipeline(PipelineStage::RECEPTION);
    message = *decoded.message;

    if (!receptionQueue.TryPush(message))
    {
        return Fail(
            context,
            "CPM_APPLICATION_QUEUE_FULL",
            "cpm_application_reception_queue_full",
            PipelineResult::DROP);
    }

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::INFO,
        "CPM",
        "CPM_RECEPTION_OK",
        MessageSummary(message));

    // DELIVER: this succeeds only after decode, semantic checks, identity
    // binding and the bounded duplicate check have all passed.
    context.AdvancePipeline(PipelineStage::DELIVER);
    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::INFO,
        "CPM",
        "CPM_DELIVER_OK",
        "received_at_unix_ms=" +
            std::to_string(message.receivedAtUnixMs) +
        " " + MessageSummary(message));

    return true;
}

} // namespace ns3
