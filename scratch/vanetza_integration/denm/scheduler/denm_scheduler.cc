#include "denm_scheduler.h"

#include "vanetza_integration/context/vehicle_dispatcher/vehicle_dispatcher.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/denm/builder/denm_builder.h"
#include "vanetza_integration/denm/service/denm_service.h"
#include "vanetza_integration/denm/sender/denm_sender.h"
#include "vanetza_integration/denm/scheduler/denm_repetition_policy.h"

#include "ns3/simulator.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{

constexpr double DENM_FIRST_EVENT_TIME_SEC = 1.0;

constexpr uint64_t DENM_REPETITION_INTERVAL_US =
    200000;

constexpr uint32_t DENM_MAX_REPETITIONS =
    3;

struct DenmScheduledEvent
{
    ns3::DenmTriggerRequest triggerRequest;
    ns3::DenmRepetitionPolicy repetitionPolicy;
};

std::unordered_map<uint32_t, DenmScheduledEvent> g_events;

ns3::DenmLifecycleManager g_lifecycle;

} // namespace

namespace ns3
{

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
            // No active scheduler state:
            // generate the baseline NEW DENM.
            //------------------------------------------------------------------

            auto eventIt =
                g_events.find(stationId);

            if (eventIt == g_events.end())
            {
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

                //------------------------------------------------------------------
                // Only after a successful NEW transmission do we create
                // scheduler repetition state.
                //------------------------------------------------------------------

                DenmScheduledEvent scheduledEvent;

                scheduledEvent.triggerRequest =
                    triggerRequest;

                scheduledEvent.repetitionPolicy.Start(
                    nowUs,
                    DENM_REPETITION_INTERVAL_US,
                    DENM_MAX_REPETITIONS);

                g_events.emplace(
                    stationId,
                    std::move(scheduledEvent));

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
            // Existing event:
            // scheduler only considers a repetition when the temporal
            // policy says it is due.
            //------------------------------------------------------------------

            DenmScheduledEvent& scheduledEvent =
                eventIt->second;

            if (!scheduledEvent.repetitionPolicy.ShouldRepeat(nowUs))
            {
                return;
            }

            DenmLifecycleResult lifecycleResult;

            if (!g_lifecycle.Repeat(
                    vehicle,
                    scheduledEvent.triggerRequest.eventId,
                    lifecycleResult))
            {
                //----------------------------------------------------------------
                // Lifecycle state no longer exists or is no longer active.
                // Scheduler state is stale and must not generate anything.
                //----------------------------------------------------------------

                g_events.erase(eventIt);
                return;
            }

            DenmGenerationContext generation =
                DenmBuilder::Build(
                    vehicle,
                    scheduledEvent.triggerRequest,
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

                return;
            }

            PacketContext& ctx =
                *generation.packetContext;

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
                    scheduledEvent.repetitionPolicy
                        .RepetitionCount() + 1));

            const auto rawPayload =
                DenmService::GenerateRawPayload(
                    generation,
                    "BASELINE_REPETITION");

            if (rawPayload.empty())
            {
                V2XWriteLog(
                    ctx,
                    V2XLogLevel::ERROR,
                    "DENM",
                    "REPETITION_GENERATE_FAIL",
                    "DENM repetition encoding failed");

                return;
            }

            const bool sent =
                DenmSender::SendRaw(
                    generation.packetContext,
                    vehicle,
                    generation.denmInput,
                    rawPayload,
                    disseminationRadiusMeters,
                    "BASELINE_REPETITION");

            if (!sent)
            {
                //----------------------------------------------------------------
                // Important:
                // a failed repetition does NOT abort the active DENM event.
                //----------------------------------------------------------------

                V2XWriteLog(
                    ctx,
                    V2XLogLevel::ERROR,
                    "DENM",
                    "REPETITION_TX_FAIL",
                    "repetition transmission failed");

                return;
            }

            scheduledEvent.repetitionPolicy.Consume(
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
                    scheduledEvent.repetitionPolicy
                        .RepetitionCount()));
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
    g_events.clear();
    g_lifecycle.Reset();
}

} // namespace ns3