#include "spatem_converter.h"

SpatemMessage
SpatemConverter::ToMessage(
    const SPATEM_t& spatem)
{
    SpatemMessage message{};

    message.stationId =
        static_cast<uint32_t>(
            spatem.header.stationID);

    for (int i = 0;
         i < spatem.spat.intersections.list.count;
         ++i)
    {
        const auto* source =
            spatem.spat.intersections.list.array[i];

        if (!source)
        {
            continue;
        }

        SpatemIntersection intersection;

        if (source->id.region)
        {
            intersection.region =
                static_cast<uint16_t>(
                    *source->id.region);
        }

        intersection.intersectionId =
            static_cast<uint16_t>(
                source->id.id);

        intersection.revision =
            static_cast<uint8_t>(
                source->revision);

        if (source->status.buf &&
            source->status.size == 2 &&
            source->status.bits_unused == 0)
        {
            intersection.statusBits =
                static_cast<uint16_t>(
                    (static_cast<uint16_t>(
                        source->status.buf[0]) << 8U) |
                    static_cast<uint16_t>(
                        source->status.buf[1]));
        }

        if (source->moy)
        {
            intersection.minuteOfYear =
                static_cast<uint32_t>(*source->moy);
        }

        if (source->timeStamp)
        {
            intersection.dSecond =
                static_cast<uint16_t>(*source->timeStamp);
        }

        for (int j = 0;
             j < source->states.list.count;
             ++j)
        {
            const auto* sourceMovement =
                source->states.list.array[j];

            if (!sourceMovement)
            {
                continue;
            }

            SpatemMovement movement;

            movement.signalGroup =
                static_cast<uint8_t>(
                    sourceMovement->signalGroup);

            for (int k = 0;
                 k < sourceMovement->state_time_speed.list.count;
                 ++k)
            {
                const auto* sourceEvent =
                    sourceMovement
                        ->state_time_speed.list.array[k];

                if (!sourceEvent)
                {
                    continue;
                }

                SpatemMovementEvent event;

                event.phaseState =
                    static_cast<SpatemMovementPhaseState>(
                        sourceEvent->eventState);

                if (sourceEvent->timing)
                {
                    const auto& sourceTiming =
                        *sourceEvent->timing;

                    SpatemTiming timing;

                    timing.minEndTime =
                        static_cast<uint32_t>(
                            sourceTiming.minEndTime);

                    if (sourceTiming.startTime)
                    {
                        timing.startTime =
                            static_cast<uint32_t>(
                                *sourceTiming.startTime);
                    }

                    if (sourceTiming.maxEndTime)
                    {
                        timing.maxEndTime =
                            static_cast<uint32_t>(
                                *sourceTiming.maxEndTime);
                    }

                    if (sourceTiming.likelyTime)
                    {
                        timing.likelyTime =
                            static_cast<uint32_t>(
                                *sourceTiming.likelyTime);
                    }

                    if (sourceTiming.confidence)
                    {
                        timing.confidence =
                            static_cast<uint8_t>(
                                *sourceTiming.confidence);
                    }

                    if (sourceTiming.nextTime)
                    {
                        timing.nextTime =
                            static_cast<uint32_t>(
                                *sourceTiming.nextTime);
                    }

                    event.timing = timing;
                }

                movement.events.push_back(event);
            }

            intersection.movements.push_back(
                movement);
        }

        message.intersections.push_back(
            intersection);
    }

    return message;
}
