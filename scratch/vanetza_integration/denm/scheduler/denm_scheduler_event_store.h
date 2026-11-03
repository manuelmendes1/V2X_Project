#pragma once

#include "vanetza_integration/denm/scheduler/denm_repetition_policy.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/denm/types/denm_event_id.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace ns3 {

    struct DenmScheduledEvent {

        DenmTriggerRequest triggerRequest;
        DenmRepetitionPolicy repetitionPolicy;
        bool initialTransmissionPending = true;
        bool lifecycleTransitionPending = false;
        uint64_t initialTransmissionEligibleAtUs = 0;

        void SetInitialTransmissionEligibility(
            uint64_t acceptedAtUs,
            uint64_t delayUs) noexcept
        {
            const uint64_t maximum =
                std::numeric_limits<uint64_t>::max();

            initialTransmissionEligibleAtUs =
                delayUs > maximum - acceptedAtUs
                    ? maximum
                    : acceptedAtUs + delayUs;
        }

        bool IsInitialTransmissionDue(
            uint64_t nowUs) const noexcept
        {
            return nowUs >= initialTransmissionEligibleAtUs;
        }
    };

    class DenmScheduledEventStore {

    public:

        bool Insert(const DenmEventId& id, const DenmScheduledEvent& event) {

            const auto [it, inserted] = m_events.emplace(id, event);

            if (!inserted){
                return false;
            }

            m_eventIdsByVehicle[id.vehicleId].push_back(id);

            return true;
        }

        DenmScheduledEvent* Find(const DenmEventId& id) {

            auto it = m_events.find(id);

            if (it == m_events.end()){
                return nullptr;
            }

            return &it->second;
        }

        bool GetTriggerRequest(const DenmEventId& id, DenmTriggerRequest& request) const {
            const auto* event = Find(id);

            if (event == nullptr)
            {
                return false;
            }

            request = event->triggerRequest;
            return true;
        }

        const DenmScheduledEvent* Find(const DenmEventId& id) const {

            auto it = m_events.find(id);

            if (it == m_events.end()) {
                return nullptr;
            }

            return &it->second;
        }

        bool IsInitialTransmissionPending(const DenmEventId& id) const {

            const auto* event = Find(id);

            return event != nullptr && event->initialTransmissionPending;
        }

        bool MarkInitialTransmissionComplete(const DenmEventId& id) {
            auto* event = Find(id);

            if (event == nullptr || !event->initialTransmissionPending) {
                return false;
            }

            event->initialTransmissionPending = false;
            return true;
        }

        std::vector<DenmEventId> GetEventIdsForVehicle(const std::string& vehicleId) const {

            const auto it = m_eventIdsByVehicle.find(vehicleId);

            if (it == m_eventIdsByVehicle.end()){
                return {};
            }

            return it->second;
        }

        bool Erase(const DenmEventId& id) {

            const auto eventIt = m_events.find(id);

            if (eventIt == m_events.end()) {
                return false;
            }

            const auto vehicleId = id.vehicleId;

            m_events.erase(eventIt);

            const auto indexIt = m_eventIdsByVehicle.find(vehicleId);

            if (indexIt != m_eventIdsByVehicle.end()) {

                auto& eventIds = indexIt->second;

                eventIds.erase(std::remove(eventIds.begin(), eventIds.end(), id), eventIds.end());

                if (eventIds.empty()) {
                    m_eventIdsByVehicle.erase(indexIt);
                }
            }

            return true;
        }

        std::size_t PendingInitialTransmissionCount() const noexcept {
            std::size_t count = 0;

            for (const auto& [id, event] : m_events) {
                (void)id;

                if (event.initialTransmissionPending) {
                    ++count;
                }
            }

            return count;
        }

        std::size_t RepetitionCount(const DenmEventId& id) const {

            const auto* event = Find(id);

            if (event == nullptr) {
                return 0;
            }

            return event->repetitionPolicy.RepetitionCount();
        }

        std::size_t Size() const noexcept {

            return m_events.size();
        }

        void Clear() noexcept {

            m_events.clear();
            m_eventIdsByVehicle.clear();
        }

        bool UpdateTriggerRequest(
            const DenmEventId& id,
            const DenmTriggerRequest& request)
        {
            auto* event = Find(id);

            if (event == nullptr)
            {
                return false;
            }

            event->triggerRequest = request;
            event->lifecycleTransitionPending = true;

            return true;
        }

        bool IsLifecycleTransitionPending(
            const DenmEventId& id) const
        {
            const auto* event = Find(id);

            return event != nullptr &&
                   event->lifecycleTransitionPending;
        }

        bool MarkLifecycleTransitionComplete(
            const DenmEventId& id)
        {
            auto* event = Find(id);

            if (event == nullptr ||
                !event->lifecycleTransitionPending)
            {
                return false;
            }

            event->lifecycleTransitionPending = false;
            return true;
        }

    private:
        std::unordered_map<DenmEventId, DenmScheduledEvent, DenmEventIdHash> m_events;

        std::unordered_map<std::string, std::vector<DenmEventId>> m_eventIdsByVehicle;
    };

} // namespace ns3
