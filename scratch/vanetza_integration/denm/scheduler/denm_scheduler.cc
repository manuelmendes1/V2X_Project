#include "denm_scheduler.h"

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_dispatcher/vehicle_dispatcher.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/denm/builder/denm_builder.h"
#include "vanetza_integration/denm/service/denm_service.h"
#include "vanetza_integration/denm/sender/denm_sender.h"
#include "vanetza_integration/denm/scheduler/denm_repetition_policy.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler_event_store.h"

#include "ns3/simulator.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>


namespace
{

constexpr double DENM_FIRST_EVENT_TIME_SEC = 1.0;

constexpr uint64_t DENM_REPETITION_INTERVAL_US =
    200000;

constexpr uint32_t DENM_MAX_REPETITIONS =
    3;

bool g_compatibilityFallbackEnabled = true;

ns3::DenmScheduledEventStore g_events;

ns3::DenmLifecycleManager g_lifecycle;

ns3::DenmScheduler::TransmissionContextObserver
    g_transmissionContextObserver;

void
NotifyTransmissionContext(
    const std::shared_ptr<ns3::PacketContext>& context)
{
    if (g_transmissionContextObserver && context)
    {
        g_transmissionContextObserver(context);
    }
}

} // namespace

namespace ns3
{

bool
DenmScheduler::Submit(
    const DenmTriggerRequest& request)
{
    VehicleState* vehicle =
        VehicleRegistry::Instance().GetByVehId(
            request.vehicleId);

    if (vehicle == nullptr)
    {
        return false;
    }

    bool callbackInvoked = false;

    DenmTrigger trigger(
        [&](const DenmTriggerRequest&)
        {
            callbackInvoked = true;
        });

    if (!trigger.Submit(request) ||
        !callbackInvoked)
    {
        return false;
    }

    const DenmEventId eventKey{
        request.vehicleId,
        request.eventId};

    DenmScheduledEvent* existing =
        g_events.Find(eventKey);

    if (existing == nullptr)
    {
        if (request.eventType !=
            DenmTriggerEventType::HAZARD_DETECTED)
        {
            return false;
        }

        DenmScheduledEvent scheduledEvent;
        scheduledEvent.triggerRequest =
            request;

        return g_events.Insert(
            eventKey,
            scheduledEvent);
    }

    //
    // An active logical event already exists.
    //
    // A second HAZARD_DETECTED is invalid because it would attempt
    // to create a second NEW lifecycle for the same EventID.
    //
    if (request.eventType ==
        DenmTriggerEventType::HAZARD_DETECTED)
    {
        return false;
    }

    //
    // UPDATE/CLEAR are lifecycle transitions of the existing EventID.
    // Do not call DenmLifecycleManager here. The scheduler records the
    // new trigger and Run() materializes the lifecycle transition.
    //
    if (existing->initialTransmissionPending)
    {
        return false;
    }

    return g_events.UpdateTriggerRequest(
        eventKey,
        request);
}

void
DenmScheduler::SetTransmissionContextObserver(
    TransmissionContextObserver observer)
{
    g_transmissionContextObserver =
        std::move(observer);
}

std::size_t
DenmScheduler::ScheduledEventCount()
{
    return g_events.Size();
}

std::size_t
DenmScheduler::PendingInitialTransmissionCount()
{
    return g_events.PendingInitialTransmissionCount();
}

std::size_t
DenmScheduler::RepetitionCount(
    const DenmEventId& eventId)
{
    return g_events.RepetitionCount(eventId);
}

bool
DenmScheduler::GetTriggerRequest(
    const DenmEventId& eventId,
    DenmTriggerRequest& request)
{
    return g_events.GetTriggerRequest(
        eventId,
        request);
}


void
DenmScheduler::Run(
    double disseminationRadiusMeters)
{
    const double now =
        Simulator::Now().GetSeconds();

    const uint64_t nowUs =
        Simulator::Now().GetMicroSeconds();

    if (now < DENM_FIRST_EVENT_TIME_SEC)
    {
        return;
    }

    if (!std::isfinite(disseminationRadiusMeters) ||
        disseminationRadiusMeters <= 0.0)
    {
        return;
    }

    VehicleDispatcher::ForEachReadyVehicle(
        [&](VehicleState& vehicle)
        {
            const uint32_t stationId =
                vehicle.identity.stationId;

            if (stationId == 0)
            {
                return;
            }

            //------------------------------------------------------------------
            // Events explicitly submitted through DenmScheduler::Submit()
            // are the authoritative scheduler state for this vehicle.
            //------------------------------------------------------------------

            const auto eventIds =
                g_events.GetEventIdsForVehicle(
                    vehicle.identity.vehId);

            //------------------------------------------------------------------
            // Compatibility fallback:
            // create the historical baseline event only when the vehicle has
            // no externally submitted scheduler events.
            //------------------------------------------------------------------

            if (eventIds.empty() &&
                g_compatibilityFallbackEnabled)
            {
                const DenmEventId eventKey{
                    vehicle.identity.vehId,
                    "baseline-event"};

                DenmTriggerRequest triggerRequest;

                triggerRequest.source =
                    DenmTriggerSource::SCENARIO;

                triggerRequest.eventType =
                    DenmTriggerEventType::HAZARD_DETECTED;

                triggerRequest.vehicleId =
                    vehicle.identity.vehId;

                triggerRequest.eventId =
                    "baseline-event";

                triggerRequest.x =
                    vehicle.mobility.position.x;

                triggerRequest.y =
                    vehicle.mobility.position.y;

                triggerRequest.causeCode =
                    94;

                triggerRequest.relevanceDistanceMeters =
                    disseminationRadiusMeters;

                triggerRequest.validityDurationMs =
                    5000;

                triggerRequest.triggerTimestampUs =
                    nowUs;

                triggerRequest.reason =
                    "BASELINE_NEW";

                DenmLifecycleResult lifecycleResult;

                bool lifecycleAccepted =
                    false;

                DenmTrigger trigger(
                    [&](const DenmTriggerRequest& request)
                    {
                        lifecycleAccepted =
                            g_lifecycle.Process(
                                vehicle,
                                request,
                                lifecycleResult);
                    });

                if (!trigger.Submit(triggerRequest) ||
                    !lifecycleAccepted)
                {
                    return;
                }

                DenmGenerationContext generation =
                    DenmBuilder::Build(
                        vehicle,
                        triggerRequest,
                        lifecycleResult);

                if (!generation.packetContext ||
                    generation.denmInput.actionId.sequenceNumber == 0)
                {
                    g_lifecycle.Abort(
                        triggerRequest.vehicleId,
                        triggerRequest.eventId);

                    return;
                }

                PacketContext& ctx =
                    *generation.packetContext;

                V2XWriteLog(
                    ctx,
                    V2XLogLevel::INFO,
                    "DENM",
                    "REQUEST",
                    "action=NEW"
                    " station=" +
                    std::to_string(
                        generation.denmInput
                            .actionId
                            .originatingStationId) +
                    " action_seq=" +
                    std::to_string(
                        generation.denmInput
                            .actionId
                            .sequenceNumber) +
                    " radius_m=" +
                    std::to_string(
                        disseminationRadiusMeters));

                const auto rawPayload =
                    DenmService::GenerateRawPayload(
                        generation,
                        "BASELINE_NEW");

                if (rawPayload.empty())
                {
                    g_lifecycle.Abort(
                        triggerRequest.vehicleId,
                        triggerRequest.eventId);

                    return;
                }

                const bool sent =
                    DenmSender::SendRaw(
                        generation.packetContext,
                        vehicle,
                        generation.denmInput,
                        rawPayload,
                        disseminationRadiusMeters,
                        "BASELINE_NEW");

                if (!sent)
                {
                    g_lifecycle.Abort(
                        triggerRequest.vehicleId,
                        triggerRequest.eventId);

                    return;
                }

                NotifyTransmissionContext(
                    generation.packetContext);

                DenmScheduledEvent scheduledEvent;

                scheduledEvent.triggerRequest = triggerRequest;

                scheduledEvent.initialTransmissionPending = false;

                scheduledEvent.repetitionPolicy.Start(
                    nowUs,
                    DENM_REPETITION_INTERVAL_US,
                    DENM_MAX_REPETITIONS);

                const bool inserted =
                    g_events.Insert(
                        eventKey,
                        scheduledEvent);

                if (!inserted)
                {
                    g_lifecycle.Abort(
                        triggerRequest.vehicleId,
                        triggerRequest.eventId);

                    return;
                }

                V2XWriteLog(
                    ctx,
                    V2XLogLevel::INFO,
                    "DENM",
                    "REPETITION_POLICY_START",
                    "interval_us=" +
                    std::to_string(
                        DENM_REPETITION_INTERVAL_US) +
                    " max_repetitions=" +
                    std::to_string(
                        DENM_MAX_REPETITIONS));

                return;
            }

            //------------------------------------------------------------------
            // Process all scheduler events belonging to this vehicle.
            //
            // A vehicle may have multiple independent logical DENM events.
            // Each event owns its own lifecycle state and repetition policy.
            //------------------------------------------------------------------

            for (const DenmEventId& eventKey : eventIds)
            {
                DenmScheduledEvent* scheduledEvent =
                    g_events.Find(eventKey);

                if (scheduledEvent == nullptr)
                {
                    continue;
                }

                if (scheduledEvent->lifecycleTransitionPending)
                {
                    const DenmTriggerRequest& triggerRequest =
                        scheduledEvent->triggerRequest;

                    DenmLifecycleResult lifecycleResult;

                    bool lifecycleAccepted =
                        false;

                    DenmTrigger trigger(
                        [&](const DenmTriggerRequest& request)
                        {
                            lifecycleAccepted =
                                g_lifecycle.Process(
                                    vehicle,
                                    request,
                                    lifecycleResult);
                        });

                    if (!trigger.Submit(triggerRequest) ||
                        !lifecycleAccepted)
                    {
                        continue;
                    }

                    DenmGenerationContext generation =
                        DenmBuilder::Build(
                            vehicle,
                            triggerRequest,
                            lifecycleResult);

                    if (!generation.packetContext ||
                        generation.denmInput.actionId.sequenceNumber == 0)
                    {
                        continue;
                    }

                    PacketContext& ctx =
                        *generation.packetContext;

                    const std::string txReason =
                        triggerRequest.reason.empty()
                            ? "DENM_LIFECYCLE"
                            : triggerRequest.reason;

                    V2XWriteLog(
                        ctx,
                        V2XLogLevel::INFO,
                        "DENM",
                        "LIFECYCLE_REQUEST",
                        "action=" +
                        std::to_string(
                            static_cast<unsigned>(
                                lifecycleResult.action)) +
                        " station=" +
                        std::to_string(
                            generation.denmInput.actionId
                                .originatingStationId) +
                        " action_seq=" +
                        std::to_string(
                            generation.denmInput.actionId
                                .sequenceNumber) +
                        " event_id=" +
                        triggerRequest.eventId);

                    const auto rawPayload =
                        DenmService::GenerateRawPayload(
                            generation,
                            txReason);

                    if (rawPayload.empty())
                    {
                        continue;
                    }

                    const bool sent =
                        DenmSender::SendRaw(
                            generation.packetContext,
                            vehicle,
                            generation.denmInput,
                            rawPayload,
                            disseminationRadiusMeters,
                            txReason);

                    if (!sent)
                    {
                        continue;
                    }

                    NotifyTransmissionContext(
                        generation.packetContext);

                    if (lifecycleResult.action ==
                        DenmManagementAction::CANCELLATION)
                    {
                        g_events.Erase(eventKey);
                    }
                    else
                    {
                        scheduledEvent =
                            g_events.Find(eventKey);

                        if (scheduledEvent != nullptr)
                        {
                            scheduledEvent->repetitionPolicy.Start(
                                nowUs,
                                DENM_REPETITION_INTERVAL_US,
                                DENM_MAX_REPETITIONS);

                            g_events.MarkLifecycleTransitionComplete(
                                eventKey);
                        }
                    }

                    continue;
                }

                //----------------------------------------------------------------
                // Initial transmission of an externally submitted event.
                //----------------------------------------------------------------

                if (scheduledEvent->initialTransmissionPending)
                {
                    const DenmTriggerRequest& triggerRequest =
                        scheduledEvent->triggerRequest;

                    DenmLifecycleResult lifecycleResult;

                    bool lifecycleAccepted =
                        false;

                    DenmTrigger trigger(
                        [&](const DenmTriggerRequest& request)
                        {
                            lifecycleAccepted =
                                g_lifecycle.Process(
                                    vehicle,
                                    request,
                                    lifecycleResult);
                        });

                    if (!trigger.Submit(triggerRequest) ||
                        !lifecycleAccepted)
                    {
                        continue;
                    }

                    DenmGenerationContext generation =
                        DenmBuilder::Build(
                            vehicle,
                            triggerRequest,
                            lifecycleResult);

                    if (!generation.packetContext ||
                        generation.denmInput.actionId.sequenceNumber == 0)
                    {
                        g_lifecycle.Abort(
                            triggerRequest.vehicleId,
                            triggerRequest.eventId);

                        continue;
                    }

                    PacketContext& ctx =
                        *generation.packetContext;

                    const std::string txReason =
                        triggerRequest.reason.empty()
                            ? "DENM_NEW"
                            : triggerRequest.reason;

                    V2XWriteLog(
                        ctx,
                        V2XLogLevel::INFO,
                        "DENM",
                        "REQUEST",
                        "action=NEW"
                        " station=" +
                        std::to_string(
                            generation.denmInput
                                .actionId
                                .originatingStationId) +
                        " action_seq=" +
                        std::to_string(
                            generation.denmInput
                                .actionId
                                .sequenceNumber) +
                        " radius_m=" +
                        std::to_string(
                            disseminationRadiusMeters) +
                        " event_id=" +
                        triggerRequest.eventId);

                    const auto rawPayload =
                        DenmService::GenerateRawPayload(
                            generation,
                            txReason);

                    if (rawPayload.empty())
                    {
                        g_lifecycle.Abort(
                            triggerRequest.vehicleId,
                            triggerRequest.eventId);

                        continue;
                    }

                    const bool sent =
                        DenmSender::SendRaw(
                            generation.packetContext,
                            vehicle,
                            generation.denmInput,
                            rawPayload,
                            disseminationRadiusMeters,
                            txReason);

                    if (!sent)
                    {
                        g_lifecycle.Abort(
                            triggerRequest.vehicleId,
                            triggerRequest.eventId);

                        continue;
                    }

                    NotifyTransmissionContext(
                        generation.packetContext);

                    scheduledEvent->repetitionPolicy.Start(
                        nowUs,
                        DENM_REPETITION_INTERVAL_US,
                        DENM_MAX_REPETITIONS);

                    const bool markedComplete =
                        g_events.MarkInitialTransmissionComplete(
                            eventKey);

                    if (!markedComplete)
                    {
                        V2XWriteLog(
                            ctx,
                            V2XLogLevel::ERROR,
                            "DENM",
                            "INITIAL_STATE_UPDATE_FAIL",
                            "successful NEW transmission but scheduler state "
                            "could not be marked complete");

                        continue;
                    }

                    V2XWriteLog(
                        ctx,
                        V2XLogLevel::INFO,
                        "DENM",
                        "REPETITION_POLICY_START",
                        "interval_us=" +
                        std::to_string(
                            DENM_REPETITION_INTERVAL_US) +
                        " max_repetitions=" +
                        std::to_string(
                            DENM_MAX_REPETITIONS) +
                        " event_id=" +
                        triggerRequest.eventId);

                    continue;
                }

                //----------------------------------------------------------------
                // Existing active event:
                // only transmit repetition when its policy says it is due.
                //----------------------------------------------------------------

                if (!scheduledEvent->repetitionPolicy.ShouldRepeat(nowUs))
                {
                    continue;
                }

                DenmLifecycleResult lifecycleResult;

                if (!g_lifecycle.Repeat(
                        vehicle,
                        scheduledEvent->triggerRequest.eventId,
                        lifecycleResult))
                {
                    g_events.Erase(eventKey);
                    continue;
                }

                DenmGenerationContext generation =
                    DenmBuilder::Build(
                        vehicle,
                        scheduledEvent->triggerRequest,
                        lifecycleResult);

                if (!generation.packetContext ||
                    generation.denmInput.actionId.sequenceNumber == 0)
                {
                    if (generation.packetContext)
                    {
                        V2XWriteLog(
                            *generation.packetContext,
                            V2XLogLevel::ERROR,
                            "DENM",
                            "REPETITION_GENERATE_FAIL",
                            "repetition generation failed");
                    }

                    continue;
                }

                PacketContext& ctx =
                    *generation.packetContext;

                const std::string repetitionReason =
                    scheduledEvent->triggerRequest.reason.empty()
                        ? "DENM_REPETITION"
                        : scheduledEvent->triggerRequest.reason +
                              "_REPETITION";

                V2XWriteLog(
                    ctx,
                    V2XLogLevel::INFO,
                    "DENM",
                    "REPETITION_REQUEST",
                    "action=REPETITION"
                    " semantic_action=" +
                    std::to_string(
                        static_cast<unsigned>(
                            lifecycleResult.action)) +
                    " station=" +
                    std::to_string(
                        generation.denmInput
                            .actionId
                            .originatingStationId) +
                    " action_seq=" +
                    std::to_string(
                        generation.denmInput
                            .actionId
                            .sequenceNumber) +
                    " repetition_index=" +
                    std::to_string(
                        scheduledEvent->repetitionPolicy
                            .RepetitionCount() + 1) +
                    " event_id=" +
                    scheduledEvent->triggerRequest.eventId);

                const auto rawPayload =
                    DenmService::GenerateRawPayload(
                        generation,
                        repetitionReason);

                if (rawPayload.empty())
                {
                    V2XWriteLog(
                        ctx,
                        V2XLogLevel::ERROR,
                        "DENM",
                        "REPETITION_GENERATE_FAIL",
                        "DENM repetition encoding failed");

                    continue;
                }

                const bool sent =
                    DenmSender::SendRaw(
                        generation.packetContext,
                        vehicle,
                        generation.denmInput,
                        rawPayload,
                        disseminationRadiusMeters,
                        repetitionReason);

                if (!sent)
                {
                    V2XWriteLog(
                        ctx,
                        V2XLogLevel::ERROR,
                        "DENM",
                        "REPETITION_TX_FAIL",
                        "repetition transmission failed");

                    continue;
                }

                NotifyTransmissionContext(
                    generation.packetContext);

                scheduledEvent->repetitionPolicy.Consume(
                    nowUs);

                V2XWriteLog(
                    ctx,
                    V2XLogLevel::INFO,
                    "DENM",
                    "REPETITION_OK",
                    "action_seq=" +
                    std::to_string(
                        generation.denmInput
                            .actionId
                            .sequenceNumber) +
                    " repetition_index=" +
                    std::to_string(
                        scheduledEvent->repetitionPolicy
                            .RepetitionCount()) +
                    " event_id=" +
                    scheduledEvent->triggerRequest.eventId);
            }
        });
}

Time
DenmScheduler::RunInterval()
{
    return MilliSeconds(10);
}

void
DenmScheduler::Reset()
{
    g_events.Clear();
    g_lifecycle.Reset();
    g_transmissionContextObserver =
        TransmissionContextObserver{};
}

void
DenmScheduler::SetCompatibilityFallbackEnabled(
    bool enabled)
{
    g_compatibilityFallbackEnabled = enabled;
}

} // namespace ns3