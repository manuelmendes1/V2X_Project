#include "vanetza_integration/vam/codec/vam_uper_encoder.h"
#include "vanetza_integration/vam/types/vam_input.h"

#include <vanetza/asn1/vam.hpp>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/TrafficParticipantType.h>
#include <vanetza/asn1/its/r2/VruProfileAndSubprofile.h>
#include <vanetza/asn1/its/r2/VruSubProfilePedestrian.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace
{

using namespace vanetza_integration::vam;

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    return condition;
}

VamInput MakeValidPedestrianInput()
{
    VamInput input{};
    input.originatingStation.stationId.value = 9001;
    input.vruProfile = VruProfile::Pedestrian;
    input.vruRole.role = vanetza_integration::vru::role::VruRoleStatus::On;
    input.vruRole.vbs = vanetza_integration::vru::role::VbsState::ActiveStandalone;

    input.generationTimeIts = std::chrono::microseconds{123456000};
    input.referencePositionTimeIts = std::chrono::microseconds{123445000};

    input.referencePosition.latitudeDeg = 41.42528995;
    input.referencePosition.longitudeDeg = -8.43407620;
    input.referencePosition.altitudeMeters = std::nullopt;

    input.highFrequencyState.headingDeg = 90.0;
    input.highFrequencyState.speedMps = 5.12;
    input.highFrequencyState.longitudinalAccelerationMps2 = -0.3;

    VamLowFrequencyAttributes lowFrequency{};
    lowFrequency.subProfileLabel = "ordinary_pedestrian";
    input.lowFrequencyAttributes = lowFrequency;

    return input;
}

VamUperEncodeResult Encode(
    const VamInput& input,
    bool includeLowFrequencyContainer)
{
    VamUperEncodeOptions options{};
    options.includeLowFrequencyContainer = includeLowFrequencyContainer;
    return VamUperEncoder::Encode(input, options);
}

bool CheckEncodedShape(const VamUperEncodeResult& encoded)
{
    const bool valid =
        encoded.success && encoded.error.empty() &&
        !encoded.bytes.empty() && encoded.encodedBits > 0 &&
        encoded.encodedBits <= encoded.bytes.size() * 8 &&
        encoded.encodedBits > (encoded.bytes.size() - 1) * 8;
    return Check(valid, "UPER_RESULT_HAS_VALID_BIT_AND_BYTE_LENGTH");
}

bool TestProfile1RoundTripAndLowFrequencyContainer()
{
    const VamInput input = MakeValidPedestrianInput();
    const VamUperEncodeResult encoded = Encode(input, true);

    bool passed = Check(
        encoded.success && encoded.error.empty(),
        "VALID_PROFILE1_INPUT_ENCODED");
    if (!encoded.success)
    {
        std::cerr << "VAM_ENCODE_ERROR=" << encoded.error << '\n';
        return false;
    }

    passed = CheckEncodedShape(encoded) && passed;

    vanetza::asn1::r2::Vam decoded;
    const bool decodedOk = decoded.decode(
        encoded.bytes.data(), encoded.bytes.size());
    passed = Check(decodedOk, "UPER_ROUNDTRIP_DECODE") && passed;
    if (!decodedOk)
    {
        return false;
    }

    const auto* root = decoded.operator->();
    if (!root)
    {
        return Check(false, "DECODED_VAM_ROOT_AVAILABLE") && passed;
    }

    passed = Check(
        root->header.protocolVersion == 3 &&
        root->header.messageId == Vanetza_ITS2_MessageId_vam &&
        root->header.stationId == 9001,
        "VAM_R2_HEADER_AND_STATION_ID_PRESERVED") && passed;

    // ETSI TS 103 300-3: GenerationDeltaTime identifies the reference
    // position timestamp, in milliseconds modulo 2^16.
    passed = Check(
        root->vam.generationDeltaTime == (123445L % 65536L),
        "REFERENCE_POSITION_TIME_ENCODED_AS_GENERATION_DELTA_TIME") && passed;

    const auto& parameters = root->vam.vamParameters;
    const auto& basic = parameters.basicContainer;
    passed = Check(
        basic.stationType == Vanetza_ITS2_TrafficParticipantType_pedestrian,
        "PROFILE1_STATION_TYPE_IS_PEDESTRIAN") && passed;

    const auto& position = basic.referencePosition;
    passed = Check(
        position.latitude == 414252900 &&
        position.longitude == -84340762,
        "WGS84_REFERENCE_POSITION_QUANTIZED") && passed;

    passed = Check(
        position.positionConfidenceEllipse.semiMajorAxisLength == 4095 &&
        position.positionConfidenceEllipse.semiMinorAxisLength == 4095 &&
        position.positionConfidenceEllipse.semiMajorAxisOrientation == 3601,
        "UNKNOWN_POSITION_ELLIPSE_EXPLICITLY_PRESERVED") && passed;

    passed = Check(
        position.altitude.altitudeValue == 800001 &&
        position.altitude.altitudeConfidence == 15,
        "UNKNOWN_ALTITUDE_EXPLICITLY_PRESERVED") && passed;

    const auto& highFrequency = parameters.vruHighFrequencyContainer;
    passed = Check(
        highFrequency.heading.value == 900 &&
        highFrequency.heading.confidence == 127,
        "HEADING_VALUE_AND_UNAVAILABLE_CONFIDENCE_PRESERVED") && passed;
    passed = Check(
        highFrequency.speed.speedValue == 512 &&
        highFrequency.speed.speedConfidence == 127,
        "SPEED_VALUE_AND_UNAVAILABLE_CONFIDENCE_PRESERVED") && passed;
    passed = Check(
        highFrequency.longitudinalAcceleration.longitudinalAccelerationValue == -3 &&
        highFrequency.longitudinalAcceleration.longitudinalAccelerationConfidence == 102,
        "LONGITUDINAL_ACCELERATION_AND_UNAVAILABLE_CONFIDENCE_PRESERVED") && passed;

    const auto* lowFrequency = parameters.vruLowFrequencyContainer;
    passed = Check(
        lowFrequency != nullptr,
        "LOW_FREQUENCY_CONTAINER_INCLUDED_WHEN_REQUESTED") && passed;
    if (lowFrequency)
    {
        passed = Check(
            lowFrequency->profileAndSubprofile.present ==
                Vanetza_ITS2_VruProfileAndSubprofile_PR_pedestrian &&
            lowFrequency->profileAndSubprofile.choice.pedestrian ==
                Vanetza_ITS2_VruSubProfilePedestrian_ordinary_pedestrian,
            "PEDESTRIAN_SUBPROFILE_MAPPED") && passed;
    }

    return passed;
}

bool TestUnavailableHighFrequencyValues()
{
    VamInput input = MakeValidPedestrianInput();
    input.highFrequencyState.headingDeg.reset();
    input.highFrequencyState.speedMps.reset();
    input.highFrequencyState.longitudinalAccelerationMps2.reset();

    const auto encoded = Encode(input, false);
    if (!encoded.success)
    {
        std::cerr << "UNAVAILABLE_FIELDS_ENCODE_ERROR=" << encoded.error << '\n';
        return Check(false, "MISSING_HF_VALUES_REPRESENTED_AS_UNAVAILABLE");
    }

    vanetza::asn1::r2::Vam decoded;
    const bool decodedOk = decoded.decode(
        encoded.bytes.data(), encoded.bytes.size());
    if (!decodedOk || !decoded.operator->())
    {
        return Check(false, "UNAVAILABLE_HF_VAM_ROUNDTRIP_DECODE");
    }

    const auto& parameters = decoded->vam.vamParameters;
    bool passed = Check(
        parameters.vruHighFrequencyContainer.heading.value == 3601 &&
        parameters.vruHighFrequencyContainer.heading.confidence == 127,
        "MISSING_HEADING_USES_UNAVAILABLE_CODES");
    passed = Check(
        parameters.vruHighFrequencyContainer.speed.speedValue == 16383 &&
        parameters.vruHighFrequencyContainer.speed.speedConfidence == 127,
        "MISSING_SPEED_USES_UNAVAILABLE_CODES") && passed;
    passed = Check(
        parameters.vruHighFrequencyContainer.longitudinalAcceleration.longitudinalAccelerationValue == 161 &&
        parameters.vruHighFrequencyContainer.longitudinalAcceleration.longitudinalAccelerationConfidence == 102,
        "MISSING_ACCELERATION_USES_UNAVAILABLE_CODES") && passed;
    passed = Check(
        parameters.vruLowFrequencyContainer == nullptr,
        "LOW_FREQUENCY_CONTAINER_OMITTED_WHEN_NOT_REQUESTED") && passed;

    return passed;
}

bool TestInvalidInputIsRejectedAtomically()
{
    VamInput missingLatitude = MakeValidPedestrianInput();
    missingLatitude.referencePosition.latitudeDeg.reset();
    const auto missingPositionResult = Encode(missingLatitude, false);

    VamInput unsupportedProfile = MakeValidPedestrianInput();
    unsupportedProfile.vruProfile = VruProfile::Motorcyclist;
    const auto unsupportedProfileResult = Encode(unsupportedProfile, false);

    VamInput staleReferencePosition = MakeValidPedestrianInput();
    staleReferencePosition.referencePositionTimeIts =
        staleReferencePosition.generationTimeIts -
        std::chrono::milliseconds{32767};
    const auto stalePositionResult = Encode(staleReferencePosition, false);

    const auto rejected = [](const VamUperEncodeResult& result) {
        return !result.success && result.bytes.empty() &&
               result.encodedBits == 0 && !result.error.empty();
    };

    bool passed = Check(
        rejected(missingPositionResult),
        "MISSING_WGS84_POSITION_REJECTED_WITHOUT_PARTIAL_PAYLOAD");
    passed = Check(
        rejected(unsupportedProfileResult),
        "NON_PROFILE1_INPUT_REJECTED_WITHOUT_PARTIAL_PAYLOAD") && passed;
    passed = Check(
        rejected(stalePositionResult),
        "REFERENCE_TIMESTAMP_OUTSIDE_32767_MS_WINDOW_REJECTED") && passed;

    return passed;
}

} // namespace

int main()
{
    bool passed = true;
    passed = TestProfile1RoundTripAndLowFrequencyContainer() && passed;
    passed = TestUnavailableHighFrequencyValues() && passed;
    passed = TestInvalidInputIsRejectedAtomically() && passed;

    if (!passed)
    {
        std::cout << "VAM_UPER_ENCODER_CONTRACT=FAIL\n";
        return 1;
    }

    std::cout << "VAM_UPER_ENCODER_CONTRACT=PASS\n";
    return 0;
}
