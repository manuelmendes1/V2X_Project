#include "denm_asn1_builder.h"

#include <cmath>
#include <cstdlib>

extern "C"
{
#include "asn_application.h"
#include "vanetza/asn1/its/SituationContainer.h"
#include "vanetza/asn1/its/Termination.h"
}

namespace
{
    constexpr double kPositionScale = 10000000.0;
}

void
DenmAsn1Builder::AsnDeleter::operator()(DENM_t* ptr) const
{
    if (ptr != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_DENM, ptr);
    }
}

DenmAsn1Builder::DenmPtr
DenmAsn1Builder::Build(const DenmInput& input)
{
    DenmPtr denm(
        static_cast<DENM_t*>(calloc(1, sizeof(DENM_t))));

    if (!denm)
    {
        return nullptr;
    }

    // ============================================================
    // ITS PDU Header
    // ============================================================

    denm->header.protocolVersion = 1;
    denm->header.messageID =
        ItsPduHeader__messageID_denm;
    denm->header.stationID =
        static_cast<StationID_t>(input.actionId.originatingStationId);

    // ============================================================
    // ManagementContainer / ActionID
    // ============================================================

    auto& management = denm->denm.management;

    management.actionID.originatingStationID =
        static_cast<StationID_t>(
            input.actionId.originatingStationId);

    management.actionID.sequenceNumber =
        static_cast<SequenceNumber_t>(
            input.actionId.sequenceNumber);

    // ============================================================
    // TimestampIts
    // ============================================================

    if (asn_uint642INTEGER(
            &management.detectionTime,
            input.detectionTimeItsMs) != 0)
    {
        return nullptr;
    }

    if (asn_uint642INTEGER(
            &management.referenceTime,
            input.referenceTimeItsMs) != 0)
    {
        return nullptr;
    }

    // ============================================================
    // ReferencePosition
    // ============================================================

    auto& position = management.eventPosition;

    position.latitude =
        static_cast<Latitude_t>(
            std::llround(
                input.eventPosition.latitude *
                kPositionScale));

    position.longitude =
        static_cast<Longitude_t>(
            std::llround(
                input.eventPosition.longitude *
                kPositionScale));

    // Unavailable position confidence is valid for the baseline.
    position.positionConfidenceEllipse.semiMajorConfidence =
        SemiAxisLength_unavailable;

    position.positionConfidenceEllipse.semiMinorConfidence =
        SemiAxisLength_unavailable;

    position.positionConfidenceEllipse.semiMajorOrientation =
        HeadingValue_unavailable;

    // Unavailable altitude is valid for the baseline.
    position.altitude.altitudeValue =
        AltitudeValue_unavailable;

    position.altitude.altitudeConfidence =
        AltitudeConfidence_unavailable;

    // ============================================================
    // StationType
    // ============================================================

    management.stationType =
        static_cast<StationType_t>(
            input.stationType);

    // ============================================================
    // Optional TransmissionInterval
    // ============================================================

    if (input.transmissionIntervalMs.has_value())
    {
        const uint32_t value =
            *input.transmissionIntervalMs;

        if (value < TransmissionInterval_oneMilliSecond ||
            value > TransmissionInterval_tenSeconds)
        {
            return nullptr;
        }

        auto* interval =
            static_cast<TransmissionInterval_t*>(
                calloc(1, sizeof(TransmissionInterval_t)));

        if (!interval)
        {
            return nullptr;
        }

        *interval =
            static_cast<TransmissionInterval_t>(value);

        management.transmissionInterval = interval;
    }

    // ============================================================
    // Action / Termination
    // ============================================================

    switch (input.action)
    {
        case DenmManagementAction::NEW:
        case DenmManagementAction::UPDATE:
            break;

        case DenmManagementAction::CANCELLATION:
        case DenmManagementAction::NEGATION:
        {
            auto* termination =
                static_cast<Termination_t*>(
                    calloc(1, sizeof(Termination_t)));

            if (!termination)
            {
                return nullptr;
            }

            *termination =
                input.action ==
                    DenmManagementAction::CANCELLATION
                    ? Termination_isCancellation
                    : Termination_isNegation;

            management.termination = termination;
            break;
        }
    }

    // ============================================================
    // Optional SituationContainer
    //
    // Build it only when the application supplied meaningful
    // situation data. The R1 ASN.1 definition allows it to be
    // absent for the minimal DENM baseline.
    // ============================================================

    if (input.informationQuality != 0 ||
        input.eventType.causeCode != 0 ||
        input.eventType.subCauseCode != 0)
    {
        auto* situation =
            static_cast<SituationContainer_t*>(
                calloc(1, sizeof(SituationContainer_t)));

        if (!situation)
        {
            return nullptr;
        }

        situation->informationQuality =
            static_cast<InformationQuality_t>(
                input.informationQuality);

        situation->eventType.causeCode =
            static_cast<CauseCodeType_t>(
                input.eventType.causeCode);

        situation->eventType.subCauseCode =
            static_cast<SubCauseCodeType_t>(
                input.eventType.subCauseCode);

        denm->denm.situation = situation;
    }

    return denm;
}
