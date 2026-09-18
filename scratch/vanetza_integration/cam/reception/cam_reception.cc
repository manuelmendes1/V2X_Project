#include "cam_reception.h"

#include "vanetza_integration/context/neighbor/neighbor_manager.h"
#include "vanetza_integration/core/v2x_log.h"

namespace ns3 {

    void CamReception::Process(PacketContext& ctx, const CamMessage& msg, VehicleState& receiver, const VehicleState& sender) {
        //======================================================================
        // RECEPTION
        //======================================================================

        //======================================================================
        // Update neighbour table
        //======================================================================

        //NeighborManager::UpdateNeighbor(receiver.neighbors, msg.stationId, msg.latitude, msg.longitude, msg.speed, msg.heading, ctx.simulationStep, sender.identity.vehId);


    }

} // namespace ns3
