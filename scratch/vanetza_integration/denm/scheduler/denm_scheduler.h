#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "ns3/nstime.h"

#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/denm/types/denm_event_id.h"

namespace ns3 {

    struct PacketContext;

    class DenmScheduler {

        public:
            using TransmissionContextObserver =
                std::function<void(std::shared_ptr<const PacketContext>)>;

            static bool Submit(const DenmTriggerRequest& request);

            static void SetInitialTransmissionDelayUs(uint64_t delayUs);

            static void SetTransmissionContextObserver(
                TransmissionContextObserver observer);

            static std::size_t ScheduledEventCount();

            static std::size_t PendingInitialTransmissionCount();

            static std::size_t RepetitionCount(const DenmEventId& eventId);

            static void Run(double disseminationRadiusMeters);

            static Time RunInterval();

            static void Reset();

            static void SetCompatibilityFallbackEnabled(bool enabled);

            static bool GetTriggerRequest(const DenmEventId& eventId, DenmTriggerRequest& request);
    };

} // namespace ns3
