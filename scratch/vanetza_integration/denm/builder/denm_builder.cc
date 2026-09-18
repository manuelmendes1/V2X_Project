#include "denm_builder.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/denm/action/denm_action_id_manager.h"

#include "ns3/simulator.h"

namespace ns3
{

DenmGenerationContext
DenmBuilder::Build(VehicleState& vehicle)
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
    // DENM ActionID is independent from PacketContext::sequenceId.
    //----------------------------------------------------------------------

    context.denmInput.action =
        DenmManagementAction::NEW;

    context.denmInput.actionId =
        DenmActionIdManager::Instance().StartAction(
            vehicle.identity.stationId);

    if (context.denmInput.actionId.sequenceNumber == 0)
    {
        V2XWriteLog(
            pkt,
            V2XLogLevel::ERROR,
            "DENM",
            "ACTION_ID_FAIL",
            "unable to allocate DENM ActionID");

        return context;
    }

    const uint64_t nowMs =
        static_cast<uint64_t>(
            now.GetMilliSeconds());

    context.denmInput.detectionTimeItsMs =
        nowMs;

    context.denmInput.referenceTimeItsMs =
        nowMs;

    //----------------------------------------------------------------------
    // Event position.
    //
    // This follows the same simulation-coordinate contract already used
    // by CAM. Conversion/quantization into ETSI ASN.1 belongs to the codec.
    //----------------------------------------------------------------------

    context.denmInput.eventPosition.latitude =
        vehicle.mobility.position.x;

    context.denmInput.eventPosition.longitude =
        vehicle.mobility.position.y;

    //----------------------------------------------------------------------
    // Minimal valid SituationContainer baseline.
    //----------------------------------------------------------------------

    context.denmInput.informationQuality = 1;

    context.denmInput.eventType.causeCode = 94;
    context.denmInput.eventType.subCauseCode = 0;

    //----------------------------------------------------------------------
    // ETSI StationType: passengerCar = 5.
    //----------------------------------------------------------------------

    context.denmInput.stationType = 5;

    return context;
}

} // namespace ns3
