#include "cam_asn1_builder.h"

#include "vanetza_integration/cam/cam_constants.h"

#include <cmath>
#include <cstdlib>

extern "C"
{
#include "asn_application.h"
}

void CamAsn1Builder::AsnDeleter::operator()(CAM_t* ptr) const
{
    if (ptr != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_CAM, ptr);
    }
}

CamAsn1Builder::CamPtr
CamAsn1Builder::Build(const CamInput& input)
{
    CamPtr cam(
        static_cast<CAM_t*>(calloc(1, sizeof(CAM_t))));

    if (!cam)
    {
        return nullptr;
    }

    // =========================================================================
    // Header
    // =========================================================================

    cam->header.protocolVersion = 1;
    cam->header.messageID = 2;
    cam->header.stationID = input.stationId;

    // =========================================================================
    // CAM
    // =========================================================================

    cam->cam.generationDeltaTime = input.generationDeltaTime;

    auto& parameters = cam->cam.camParameters;

    // -------------------------------------------------------------------------
    // Basic Container
    // -------------------------------------------------------------------------

    parameters.basicContainer.stationType =
        StationType_passengerCar;

    parameters.basicContainer.referencePosition.latitude =
        static_cast<long>(
            std::llround(
                input.latitude * kCamPositionScale));

    parameters.basicContainer.referencePosition.longitude =
        static_cast<long>(
            std::llround(
                input.longitude * kCamPositionScale));

    // -------------------------------------------------------------------------
    // High Frequency Container
    // -------------------------------------------------------------------------

    parameters.highFrequencyContainer.present =
        HighFrequencyContainer_PR_basicVehicleContainerHighFrequency;

    auto& hf =
        parameters.highFrequencyContainer
            .choice.basicVehicleContainerHighFrequency;

    hf.speed.speedValue =
        static_cast<long>(input.speed * 100);

    hf.speed.speedConfidence =
        SpeedConfidence_unavailable;

    hf.heading.headingValue =
        static_cast<long>(input.heading * 10);

    hf.heading.headingConfidence =
        HeadingConfidence_unavailable;

    hf.driveDirection =
        DriveDirection_forward;

    hf.longitudinalAcceleration.longitudinalAccelerationValue =
        static_cast<long>(input.acceleration * 10);

    hf.longitudinalAcceleration.longitudinalAccelerationConfidence =
        1;

    hf.curvature.curvatureValue = 0;

    hf.curvature.curvatureConfidence =
        CurvatureConfidence_onePerMeter_0_00002;

    hf.yawRate.yawRateValue = 0;
    hf.yawRate.yawRateConfidence = 1;

    hf.vehicleLength.vehicleLengthValue = 50;

    hf.vehicleLength.vehicleLengthConfidenceIndication =
        VehicleLengthConfidenceIndication_noTrailerPresent;

    hf.vehicleWidth = 20;

    return cam;
}
