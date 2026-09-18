#include "cam_converter.h"

#include "vanetza_integration/cam/cam_constants.h"

#include "vanetza/asn1/its/BasicContainer.h"
#include "vanetza/asn1/its/BasicVehicleContainerHighFrequency.h"

CamMessage CamConverter::ToMessage(const CAM_t& cam, uint64_t timestamp) {

    CamMessage msg{};

    msg.stationId = cam.header.stationID;

    const auto& pos = cam.cam.camParameters.basicContainer.referencePosition;

    msg.latitude = pos.latitude / kCamPositionScale;

    msg.longitude = pos.longitude / kCamPositionScale;

    const auto& hf = cam.cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency;

    msg.speed = hf.speed.speedValue / 100.0;

    msg.heading = hf.heading.headingValue / 10.0;

    msg.acceleration = hf.longitudinalAcceleration.longitudinalAccelerationValue / 10.0;

    // -------------------------------------------------------------------------
    // ETSI GenerationDeltaTime
    // -------------------------------------------------------------------------

    msg.generationDeltaTime = static_cast<uint16_t>(cam.cam.generationDeltaTime);

    msg.timestamp = timestamp;

    return msg;
}
