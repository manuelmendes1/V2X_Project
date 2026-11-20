#include "denm_builder.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"
#include "vanetza_integration/core/v2x_log.h"

#include "ns3/simulator.h"

namespace ns3
{

DenmGenerationContext
DenmBuilder::Build(
    VehicleState& vehicle,
    const DenmTriggerRequest& request,
    const DenmLifecycleResult& lifecycleResult)
{
    DenmGenerationContext context{};

    context.packetContext =
        std::make_shared<PacketContext>();

    PacketContext& pkt =
        *context.packetContext;

    const auto now =
        Simulator::Now();

    pkt.simulationStep =
        SimulationContext::simulationStep;

    pkt.simulationTime =
        now.GetSeconds();

    pkt.timestamp =
        now.GetMicroSeconds();

    pkt.senderVehId =
        vehicle.identity.vehId;

    pkt.senderNodeId =
        vehicle.identity.nodeId;

    pkt.senderStationId =
        vehicle.identity.stationId;

    pkt.ClearExecutionState();
    pkt.payload.Clear();

    //----------------------------------------------------------------------
    // DENM lifecycle is decided before generation.
    //
    // Builder MUST NOT allocate ActionID.
    // DenmLifecycleManager is the lifecycle authority.
    //----------------------------------------------------------------------

    if (request.vehicleId != vehicle.identity.vehId ||
        lifecycleResult.actionId.originatingStationId !=
            vehicle.identity.stationId ||
        lifecycleResult.actionId.sequenceNumber == 0)
    {
        V2XWriteLog(
            pkt,
            V2XLogLevel::ERROR,
            "DENM",
            "LIFECYCLE_INPUT_FAIL",
            "trigger/lifecycle identity mismatch");

        return context;
    }

    context.denmInput.action =
        lifecycleResult.action;

    context.denmInput.actionId =
        lifecycleResult.actionId;

    const uint64_t triggerMs =
        request.triggerTimestampUs / 1000;

    if (triggerMs == 0)
    {
        V2XWriteLog(
            pkt,
            V2XLogLevel::ERROR,
            "DENM",
            "TRIGGER_TIME_FAIL",
            "trigger timestamp unavailable");

        return context;
    }

    context.denmInput.detectionTimeItsMs =
        triggerMs;

    context.denmInput.referenceTimeItsMs =
        triggerMs;

    //----------------------------------------------------------------------
    // Event position.
    //
    // This follows the same simulation-coordinate contract already used
    // by CAM. Conversion/quantization into ETSI ASN.1 belongs to the codec.
    //----------------------------------------------------------------------

    // Resolve the event using the same WGS84 authority as CAM and
    // GeoNetworking. Local Cartesian metres must never be treated as degrees.
    Wgs84Position geodetic{};
    std::string geodeticError;
    if (!SumoGeodeticTransform::ResolveWgs84(
            request.geodeticPosition,
            SimulationContext::sumoProjection,
            SumoProjectedPosition{request.x, request.y},
            geodetic,
            geodeticError))
    {
        pkt.MarkFailure();
        pkt.reason = "invalid_event_geodetic_position";
        context.denmInput.actionId.sequenceNumber = 0;

        V2XWriteLog(
            pkt,
            V2XLogLevel::ERROR,
            "DENM",
            "POSITION_RESOLUTION_FAIL",
            geodeticError);
        return context;
    }

    context.denmInput.eventPosition.latitude = geodetic.latitudeDeg;
    context.denmInput.eventPosition.longitude = geodetic.longitudeDeg;

    //----------------------------------------------------------------------
    // Minimal valid SituationContainer baseline.
    //----------------------------------------------------------------------

    context.denmInput.informationQuality = 1;

    context.denmInput.eventType.causeCode =
        static_cast<uint16_t>(request.causeCode);

    context.denmInput.eventType.subCauseCode =
        0;

    // The trigger/lifecycle layer keeps validityDurationMs as event
    // metadata, but the current DENM 8.3 codec deliberately does not
    // encode this optional field yet.
    context.denmInput.validityDurationMs.reset();

    //----------------------------------------------------------------------
    // ETSI StationType: passengerCar = 5.
    //----------------------------------------------------------------------

    context.denmInput.stationType = 5;

    return context;
}

} // namespace ns3
