#include "cam_scheduler.h"

#include "vanetza_integration/cam/builder/cam_builder.h"
#include "vanetza_integration/cam/sender/cam_sender.h"
#include "vanetza_integration/cam/service/cam_service.h"
#include "vanetza_integration/cam/transmission/cam_transmission.h"

#include "vanetza_integration/context/vehicle_dispatcher/vehicle_dispatcher.h"

#include "ns3/simulator.h"

namespace ns3 {

    void CamScheduler::Run() {

        const double now = Simulator::Now().GetSeconds();

        VehicleDispatcher::ForEachReadyVehicle([now](VehicleState& vehicle){

            CamTransmissionRequest request = CamTransmission::PrepareTransmission(vehicle, now);

            if (!request.shouldSend){
                return;
            }

            CamGenerationContext context = CamBuilder::Build(*request.vehicle);

            std::vector<uint8_t> rawPayload = CamService::GenerateRawPayload(context, request.reason);

            if (rawPayload.empty()){
                return;
            }

            CamSender::SendRaw(context.packetContext, *request.vehicle, rawPayload, request.reason);

            CamTransmission::CommitTransmission(*request.vehicle, now);
        });
    }

    Time CamScheduler::RunInterval() {

        if (CamTransmission::HighRateTestEnabled()){
            return Seconds(CamTransmission::TestSchedulerInterval());
        }

        /*
         * Preserve the existing normal scheduler cadence.
         */
        return MilliSeconds(10);
    }

} // namespace ns3
