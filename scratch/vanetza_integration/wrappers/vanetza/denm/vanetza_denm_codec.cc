#include "vanetza_denm_codec.h"

#include "denm_asn1_builder.h"
#include "uper/denm_uper_decoder.h"
#include "uper/denm_uper_encoder.h"

extern "C"
{
#include "vanetza/asn1/support/INTEGER.h"
}

namespace
{
    bool HasUnsupportedBaselineFields(const DenmInput& input)
    {
        return input.validityDurationMs.has_value() ||
               input.location.eventSpeedMps.has_value() ||
               input.location.eventHeadingDeg.has_value();
    }
}

VanetzaDenmEncodeResult
VanetzaDenmCodec::Encode(const DenmInput& input)
{
    VanetzaDenmEncodeResult result;

    // Deliberately not encoded in 8.3:
    // - ValidityDuration unit has not yet been normalized to the
    //   neutral DenmInput representation.
    // - LocationContainer requires its own mandatory Traces structure.
    if (HasUnsupportedBaselineFields(input))
    {
        result.failedType = "unsupported_8_3_optional_fields";
        return result;
    }

    auto denm = DenmAsn1Builder::Build(input);

    if (!denm)
    {
        result.failedType = "asn1_build_failed";
        return result;
    }

    const auto encoded =
        DenmUperEncoder::Encode(*denm);

    if (!encoded.success)
    {
        result.failedType =
            encoded.failedType;

        return result;
    }

    result.success = true;
    result.bytes = encoded.bytes;

    return result;
}

VanetzaDenmDecodeResult
VanetzaDenmCodec::Decode(
    const std::vector<uint8_t>& payload)
{
    VanetzaDenmDecodeResult result;

    const auto decoded =
        DenmUperDecoder::Decode(payload);

    if (!decoded.success || !decoded.denm)
    {
        result.error =
            decoded.error.empty()
                ? "ASN.1 decode failed"
                : decoded.error;

        return result;
    }

    auto& message = result.message;

    message.action =
        DenmManagementAction::NEW;

    if (decoded.denm->denm.management.termination)
    {
        if (*decoded.denm->denm.management.termination ==
            Termination_isCancellation)
        {
            message.action =
                DenmManagementAction::CANCELLATION;
        }
        else if (*decoded.denm->denm.management.termination ==
                 Termination_isNegation)
        {
            message.action =
                DenmManagementAction::NEGATION;
        }
    }

    message.actionId.originatingStationId =
        static_cast<uint32_t>(
            decoded.denm->denm.management.actionID
                .originatingStationID);

    message.actionId.sequenceNumber =
        static_cast<uint16_t>(
            decoded.denm->denm.management.actionID
                .sequenceNumber);

    if (asn_INTEGER2uint64(
            &decoded.denm->denm.management.detectionTime,
            &message.detectionTimeItsMs) != 0)
    {
        result.error = "invalid detectionTime";
        return result;
    }

    if (asn_INTEGER2uint64(
            &decoded.denm->denm.management.referenceTime,
            &message.referenceTimeItsMs) != 0)
    {
        result.error = "invalid referenceTime";
        return result;
    }

    const auto& position =
        decoded.denm->denm.management.eventPosition;

    message.eventPosition.latitude =
        static_cast<double>(position.latitude) /
        10000000.0;

    message.eventPosition.longitude =
        static_cast<double>(position.longitude) /
        10000000.0;

    message.stationType =
        static_cast<uint32_t>(
            decoded.denm->denm.management.stationType);

    if (decoded.denm->denm.situation)
    {
        const auto& situation =
            *decoded.denm->denm.situation;

        message.informationQuality =
            static_cast<uint8_t>(
                situation.informationQuality);

        message.eventType.causeCode =
            static_cast<uint16_t>(
                situation.eventType.causeCode);

        message.eventType.subCauseCode =
            static_cast<uint16_t>(
                situation.eventType.subCauseCode);
    }

    if (decoded.denm->denm.management.transmissionInterval)
    {
        message.transmissionIntervalMs =
            static_cast<uint32_t>(
                *decoded.denm->denm.management
                    .transmissionInterval);
    }

    result.success = true;

    return result;
}
