#include "denm_scheduler.h"

#include "vanetza_integration/context/vehicle_dispatcher/vehicle_dispatcher.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/denm/action/denm_action_id_manager.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"
#include "vanetza_integration/denm/builder/denm_builder.h"
#include "vanetza_integration/denm/service/denm_service.h"
#include "vanetza_integration/denm/sender/denm_sender.h"

#include "ns3/simulator.h"

#include <cmath>
#include <string>
#include <unordered_set>

namespace
{

constexpr double DENM_FIRST_EVENT_TIME_SEC = 1.0;

std::unordered_set<uint32_t> g_sentStations;

DenmLifecycleManager g_lifecycle;

} // namespace

namespace ns3
{

void
DenmScheduler::Run(
    double disseminationRadiusMeters)
{
    const double now =
        Simulator::Now().GetSeconds();

    //----------------------------------------------------------------------
    // Baseline policy:
    //
    // one deterministic NEW DENM per initialized vehicle, starting at
    // simulation time 1.0 s.
    //
    // This is test/application policy, not protocol/adapter policy.
    //----------------------------------------------------------------------

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

            if (g_sentStations.find(stationId) !=
                g_sentStations.end())
            {
                return;
            }

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
                Simulator::Now().GetMicroSeconds();

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

            if (sent)
            {
                g_sentStations.insert(
                    stationId);
            }
            else
            {
                g_lifecycle.Abort(
                    triggerRequest.vehicleId,
                    triggerRequest.eventId);
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
    g_sentStations.clear();
    g_lifecycle.Reset();
    DenmActionIdManager::Instance().Reset();
}

} // namespace ns3
