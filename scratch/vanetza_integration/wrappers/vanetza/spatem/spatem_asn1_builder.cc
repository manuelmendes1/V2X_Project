#include "spatem_asn1_builder.h"

#include <cstdlib>

extern "C"
{
#include "asn_application.h"
#include "vanetza/asn1/its/ItsPduHeader.h"
}

namespace
{

bool AddStatusBits(
    IntersectionStatusObject_t& status,
    uint16_t bits)
{
    // IntersectionStatusObject is exactly 16 bits.
    // Bits 14 and 15 are reserved.
    if ((bits & 0xC000U) != 0)
    {
        return false;
    }

    status.buf =
        static_cast<uint8_t*>(calloc(2, sizeof(uint8_t)));

    if (!status.buf)
    {
        return false;
    }

    status.size = 2;
    status.bits_unused = 0;

    status.buf[0] =
        static_cast<uint8_t>((bits >> 8U) & 0xFFU);
    status.buf[1] =
        static_cast<uint8_t>(bits & 0xFFU);

    return true;
}

bool SetTimeMark(
    TimeMark_t* target,
    uint32_t value)
{
    if (!target || value > 36001U)
    {
        return false;
    }

    *target =
        static_cast<TimeMark_t>(value);

    return true;
}

bool SetOptionalTimeMark(
    TimeMark_t*& target,
    const std::optional<uint32_t>& value)
{
    if (!value.has_value())
    {
        return true;
    }

    if (*value > 36001U)
    {
        return false;
    }

    target =
        static_cast<TimeMark_t*>(
            calloc(1, sizeof(TimeMark_t)));

    if (!target)
    {
        return false;
    }

    return SetTimeMark(target, *value);
}

bool SetTiming(
    const SpatemTiming& input,
    TimeChangeDetails_t*& target)
{
    if (input.minEndTime > 36001U)
    {
        return false;
    }

    if (input.confidence.has_value() &&
        *input.confidence > 15U)
    {
        return false;
    }

    auto* timing =
        static_cast<TimeChangeDetails_t*>(
            calloc(1, sizeof(TimeChangeDetails_t)));

    if (!timing)
    {
        return false;
    }

    timing->minEndTime =
        static_cast<TimeMark_t>(
            input.minEndTime);

    if (!SetOptionalTimeMark(
            timing->startTime,
            input.startTime) ||
        !SetOptionalTimeMark(
            timing->maxEndTime,
            input.maxEndTime) ||
        !SetOptionalTimeMark(
            timing->likelyTime,
            input.likelyTime) ||
        !SetOptionalTimeMark(
            timing->nextTime,
            input.nextTime))
    {
        ASN_STRUCT_FREE(
            asn_DEF_TimeChangeDetails,
            timing);
        return false;
    }

    if (input.confidence.has_value())
    {
        timing->confidence =
            static_cast<TimeIntervalConfidence_t*>(
                calloc(
                    1,
                    sizeof(TimeIntervalConfidence_t)));

        if (!timing->confidence)
        {
            ASN_STRUCT_FREE(
                asn_DEF_TimeChangeDetails,
                timing);
            return false;
        }

        *timing->confidence =
            static_cast<TimeIntervalConfidence_t>(
                *input.confidence);
    }

    target = timing;
    return true;
}

bool BuildMovement(
    const SpatemMovement& input,
    MovementState_t& movement)
{
    if (input.events.empty() || input.events.size() > 16)
    {
        return false;
    }

    if (input.signalGroup > 255)
    {
        return false;
    }

    movement.signalGroup =
        static_cast<SignalGroupID_t>(
            input.signalGroup);

    for (const auto& inputEvent : input.events)
    {
        const auto phase =
            static_cast<uint8_t>(
                inputEvent.phaseState);

        if (phase > 9)
        {
            return false;
        }

        auto* event =
            static_cast<MovementEvent_t*>(
                calloc(1, sizeof(MovementEvent_t)));

        if (!event)
        {
            return false;
        }

        event->eventState =
            static_cast<MovementPhaseState_t>(phase);

        if (inputEvent.timing.has_value())
        {
            if (!SetTiming(
                    *inputEvent.timing,
                    event->timing))
            {
                ASN_STRUCT_FREE(
                    asn_DEF_MovementEvent,
                    event);
                return false;
            }
        }

        if (ASN_SEQUENCE_ADD(
                &movement.state_time_speed.list,
                event) != 0)
        {
            ASN_STRUCT_FREE(
                asn_DEF_MovementEvent,
                event);
            return false;
        }
    }

    return true;
}

bool BuildIntersection(
    const SpatemIntersection& input,
    IntersectionState_t& intersection)
{
    if (input.movements.empty() ||
        input.movements.size() > 255)
    {
        return false;
    }

    if (input.statusBits & 0xC000U)
    {
        return false;
    }

    intersection.id.id =
        static_cast<IntersectionID_t>(
            input.intersectionId);

    intersection.revision =
        static_cast<MsgCount_t>(
            input.revision);

    if (!AddStatusBits(
            intersection.status,
            input.statusBits))
    {
        return false;
    }

    if (input.region.has_value())
    {
        auto* region =
            static_cast<RoadRegulatorID_t*>(
                calloc(1, sizeof(RoadRegulatorID_t)));

        if (!region)
        {
            return false;
        }

        *region =
            static_cast<RoadRegulatorID_t>(
                *input.region);

        intersection.id.region = region;
    }

    if (input.minuteOfYear.has_value())
    {
        auto* value =
            static_cast<MinuteOfTheYear_t*>(
                calloc(1, sizeof(MinuteOfTheYear_t)));

        if (!value)
        {
            return false;
        }

        *value =
            static_cast<MinuteOfTheYear_t>(
                *input.minuteOfYear);

        intersection.moy = value;
    }

    if (input.dSecond.has_value())
    {
        auto* value =
            static_cast<DSecond_t*>(
                calloc(1, sizeof(DSecond_t)));

        if (!value)
        {
            return false;
        }

        *value =
            static_cast<DSecond_t>(
                *input.dSecond);

        intersection.timeStamp = value;
    }

    for (const auto& inputMovement : input.movements)
    {
        auto* movement =
            static_cast<MovementState_t*>(
                calloc(1, sizeof(MovementState_t)));

        if (!movement)
        {
            return false;
        }

        if (!BuildMovement(
                inputMovement,
                *movement))
        {
            ASN_STRUCT_FREE(
                asn_DEF_MovementState,
                movement);
            return false;
        }

        if (ASN_SEQUENCE_ADD(
                &intersection.states.list,
                movement) != 0)
        {
            ASN_STRUCT_FREE(
                asn_DEF_MovementState,
                movement);
            return false;
        }
    }

    return true;
}

} // namespace

void
SpatemAsn1Builder::AsnDeleter::operator()(
    SPATEM_t* ptr) const
{
    if (ptr)
    {
        ASN_STRUCT_FREE(
            asn_DEF_SPATEM,
            ptr);
    }
}

SpatemAsn1Builder::SpatemPtr
SpatemAsn1Builder::Build(
    const SpatemInput& input)
{
    SpatemPtr spatem(
        static_cast<SPATEM_t*>(
            calloc(1, sizeof(SPATEM_t))));

    if (!spatem)
    {
        return nullptr;
    }

    if (input.intersections.empty() ||
        input.intersections.size() > 32)
    {
        return nullptr;
    }

    spatem->header.protocolVersion = 2;
    spatem->header.messageID =
        ItsPduHeader__messageID_spatem;
    spatem->header.stationID =
        static_cast<StationID_t>(
            input.stationId);

    for (const auto& inputIntersection :
         input.intersections)
    {
        auto* intersection =
            static_cast<IntersectionState_t*>(
                calloc(1, sizeof(IntersectionState_t)));

        if (!intersection)
        {
            return nullptr;
        }

        if (!BuildIntersection(
                inputIntersection,
                *intersection))
        {
            ASN_STRUCT_FREE(
                asn_DEF_IntersectionState,
                intersection);
            return nullptr;
        }

        if (ASN_SEQUENCE_ADD(
                &spatem->spat.intersections.list,
                intersection) != 0)
        {
            ASN_STRUCT_FREE(
                asn_DEF_IntersectionState,
                intersection);
            return nullptr;
        }
    }

    return spatem;
}
