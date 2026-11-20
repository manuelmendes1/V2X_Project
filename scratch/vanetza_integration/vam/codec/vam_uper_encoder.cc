#include "vanetza_integration/vam/codec/vam_uper_encoder.h"

#include "vanetza_integration/vam/codec/vam_header_validator.h"

#include <vanetza/asn1/vam.hpp>
#include <vanetza/asn1/its/r2/AccelerationConfidence.h>
#include <vanetza/asn1/its/r2/AltitudeConfidence.h>
#include <vanetza/asn1/its/r2/AltitudeValue.h>
#include <vanetza/asn1/its/r2/GenerationDeltaTime.h>
#include <vanetza/asn1/its/r2/HeadingValue.h>
#include <vanetza/asn1/its/r2/Latitude.h>
#include <vanetza/asn1/its/r2/Longitude.h>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/LongitudinalAccelerationValue.h>
#include <vanetza/asn1/its/r2/SemiAxisLength.h>
#include <vanetza/asn1/its/r2/SpeedConfidence.h>
#include <vanetza/asn1/its/r2/SpeedValue.h>
#include <vanetza/asn1/its/r2/TrafficParticipantType.h>
#include <vanetza/asn1/its/r2/VruProfileAndSubprofile.h>
#include <vanetza/asn1/its/r2/VruSubProfilePedestrian.h>
#include <vanetza/asn1/its/r2/Wgs84AngleConfidence.h>
#include <vanetza/asn1/its/r2/Wgs84AngleValue.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

extern "C"
{
#include "asn_application.h"
#include "constr_TYPE.h"
#include "per_encoder.h"
}

namespace vanetza_integration
{
namespace vam
{
namespace
{

constexpr long kVamProtocolVersion = 3;
constexpr std::int64_t kMaxTimestampItsMs = 4398046511103LL;
constexpr std::int64_t kGenerationDeltaTimeModulus = 65536;
constexpr std::int64_t kMaxReferencePositionAgeMs = 32767;
constexpr std::size_t kMaxEncodedBufferSize = 65536;
constexpr long kGeodeticScale = 10000000L;

VamUperEncodeResult Reject(std::string error)
{
    VamUperEncodeResult result;
    result.error = std::move(error);
    return result;
}

bool ToTimestampItsMs(
    std::chrono::microseconds timestamp,
    std::int64_t& output,
    const char* name,
    std::string& error)
{
    if (timestamp.count() < 0)
    {
        error = std::string(name) + " must not be negative";
        return false;
    }

    // The VAM model deliberately stores ITS time in microseconds. The ASN.1
    // TimestampIts domain is milliseconds, so this uses the project's
    // established microseconds-to-milliseconds truncation convention.
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count();

    if (milliseconds < 0 || milliseconds > kMaxTimestampItsMs)
    {
        error = std::string(name) + " is outside the TimestampIts range";
        return false;
    }

    output = milliseconds;
    return true;
}

bool QuantizeGeodeticCoordinate(
    double degrees,
    double minimumDegrees,
    double maximumDegrees,
    bool longitude,
    long& output,
    std::string& error)
{
    if (!std::isfinite(degrees) ||
        degrees < minimumDegrees ||
        degrees > maximumDegrees)
    {
        error = "WGS84 coordinate is not finite or is out of range";
        return false;
    }

    // ETSI CDD reserves -180 degrees as Longitude.valueNotUsed.
    if (longitude && degrees == -180.0)
    {
        error = "Longitude -180 degrees maps to the reserved valueNotUsed code";
        return false;
    }

    // Scale in the input precision before rounding. Promoting a decimal
    // coordinate held in double to long double before multiplication can
    // expose its binary representation error and move an intended half-unit
    // boundary to the adjacent CDD coordinate unit.
    const double scaled = degrees * static_cast<double>(kGeodeticScale);
    const long double rounded = std::round(scaled);

    if (rounded < static_cast<long double>(std::numeric_limits<long>::min()) ||
        rounded > static_cast<long double>(std::numeric_limits<long>::max()))
    {
        error = "WGS84 coordinate overflows the ASN.1 integer type";
        return false;
    }

    output = static_cast<long>(rounded);

    if ((!longitude &&
         (output < -900000000L || output > 900000000L)) ||
        (longitude &&
         (output <= -1800000000L || output > 1800000000L)))
    {
        error = "Quantized WGS84 coordinate collides with a reserved code";
        return false;
    }

    error.clear();
    return true;
}

bool SetReferencePosition(
    const VamInput& input,
    Vanetza_ITS2_ReferencePositionWithConfidence_t& output,
    std::string& error)
{
    if (!input.referencePosition.latitudeDeg.has_value() ||
        !input.referencePosition.longitudeDeg.has_value())
    {
        error = "VAM WGS84 latitude and longitude are required";
        return false;
    }

    long latitude = 0;
    if (!QuantizeGeodeticCoordinate(
            *input.referencePosition.latitudeDeg,
            -90.0,
            90.0,
            false,
            latitude,
            error))
    {
        error = "Invalid VAM latitude: " + error;
        return false;
    }

    long longitude = 0;
    if (!QuantizeGeodeticCoordinate(
            *input.referencePosition.longitudeDeg,
            -180.0,
            180.0,
            true,
            longitude,
            error))
    {
        error = "Invalid VAM longitude: " + error;
        return false;
    }

    output.latitude = static_cast<Vanetza_ITS2_Latitude_t>(latitude);
    output.longitude = static_cast<Vanetza_ITS2_Longitude_t>(longitude);

    output.positionConfidenceEllipse.semiMajorAxisLength =
        Vanetza_ITS2_SemiAxisLength_unavailable;
    output.positionConfidenceEllipse.semiMinorAxisLength =
        Vanetza_ITS2_SemiAxisLength_unavailable;
    output.positionConfidenceEllipse.semiMajorAxisOrientation =
        Vanetza_ITS2_HeadingValue_unavailable;

    if (!input.referencePosition.altitudeMeters.has_value())
    {
        output.altitude.altitudeValue =
            Vanetza_ITS2_AltitudeValue_unavailable;
        output.altitude.altitudeConfidence =
            Vanetza_ITS2_AltitudeConfidence_unavailable;
        error.clear();
        return true;
    }

    const double altitudeMeters = *input.referencePosition.altitudeMeters;
    if (!std::isfinite(altitudeMeters))
    {
        error = "VAM altitude must be finite";
        return false;
    }

    // AltitudeValue is expressed in centimetres. The boundary codes -100000
    // and 800000 are out-of-range markers; 800001 means unavailable.
    const long double altitudeCm =
        std::round(static_cast<long double>(altitudeMeters) * 100.0L);
    if (altitudeCm <= -100000.0L || altitudeCm >= 800000.0L)
    {
        error = "VAM altitude is outside the representable physical range";
        return false;
    }

    output.altitude.altitudeValue =
        static_cast<Vanetza_ITS2_AltitudeValue_t>(altitudeCm);
    output.altitude.altitudeConfidence =
        Vanetza_ITS2_AltitudeConfidence_unavailable;
    error.clear();
    return true;
}

bool SetHeading(
    const std::optional<double>& headingDeg,
    Vanetza_ITS2_Wgs84Angle_t& output,
    std::string& error)
{
    output.confidence = Vanetza_ITS2_Wgs84AngleConfidence_unavailable;

    if (!headingDeg.has_value())
    {
        output.value = Vanetza_ITS2_Wgs84AngleValue_unavailable;
        error.clear();
        return true;
    }

    if (!std::isfinite(*headingDeg) ||
        *headingDeg < 0.0 || *headingDeg >= 360.0)
    {
        error = "VAM heading must be finite and within [0, 360) degrees";
        return false;
    }

    const long quantized = static_cast<long>(std::round(*headingDeg * 10.0));
    // Rounding values immediately below 360 degrees to 3600 is a valid
    // wrap-around to north, not the reserved doNotUse value.
    output.value = static_cast<Vanetza_ITS2_Wgs84AngleValue_t>(
        quantized == 3600L ? 0L : quantized);
    error.clear();
    return true;
}

bool SetSpeed(
    const std::optional<double>& speedMps,
    Vanetza_ITS2_Speed_t& output,
    std::string& error)
{
    output.speedConfidence = Vanetza_ITS2_SpeedConfidence_unavailable;

    if (!speedMps.has_value())
    {
        output.speedValue = Vanetza_ITS2_SpeedValue_unavailable;
        error.clear();
        return true;
    }

    if (!std::isfinite(*speedMps) ||
        *speedMps < 0.0 || *speedMps > 163.81)
    {
        error = "VAM speed must be finite and within [0, 163.81] m/s";
        return false;
    }

    const long quantized = static_cast<long>(std::round(*speedMps * 100.0));
    // 16382 and 16383 are reserved for outOfRange and unavailable.
    if (quantized > 16381L)
    {
        error = "VAM speed exceeds the representable physical range";
        return false;
    }

    output.speedValue = static_cast<Vanetza_ITS2_SpeedValue_t>(quantized);
    error.clear();
    return true;
}

bool SetLongitudinalAcceleration(
    const std::optional<double>& accelerationMps2,
    Vanetza_ITS2_LongitudinalAcceleration_t& output,
    std::string& error)
{
    output.longitudinalAccelerationConfidence =
        Vanetza_ITS2_AccelerationConfidence_unavailable;

    if (!accelerationMps2.has_value())
    {
        output.longitudinalAccelerationValue =
            Vanetza_ITS2_LongitudinalAccelerationValue_unavailable;
        error.clear();
        return true;
    }

    if (!std::isfinite(*accelerationMps2) ||
        *accelerationMps2 < -15.9 || *accelerationMps2 > 15.9)
    {
        error = "VAM longitudinal acceleration must be finite and within [-15.9, 15.9] m/s^2";
        return false;
    }

    const long quantized =
        static_cast<long>(std::round(*accelerationMps2 * 10.0));
    // -160, +160 and +161 are reserved boundary/unavailable codes.
    if (quantized < -159L || quantized > 159L)
    {
        error = "VAM longitudinal acceleration exceeds the representable physical range";
        return false;
    }

    output.longitudinalAccelerationValue =
        static_cast<Vanetza_ITS2_LongitudinalAccelerationValue_t>(quantized);
    error.clear();
    return true;
}

bool MapPedestrianSubProfile(
    const VamInput& input,
    long& output,
    std::string& error)
{
    output = Vanetza_ITS2_VruSubProfilePedestrian_unavailable;

    if (!input.lowFrequencyAttributes.has_value() ||
        !input.lowFrequencyAttributes->subProfileLabel.has_value())
    {
        error.clear();
        return true;
    }

    const std::string& label =
        *input.lowFrequencyAttributes->subProfileLabel;
    if (label == "unavailable")
    {
        output = Vanetza_ITS2_VruSubProfilePedestrian_unavailable;
    }
    else if (label == "ordinary_pedestrian")
    {
        output = Vanetza_ITS2_VruSubProfilePedestrian_ordinary_pedestrian;
    }
    else if (label == "road_worker")
    {
        output = Vanetza_ITS2_VruSubProfilePedestrian_road_worker;
    }
    else if (label == "first_responder")
    {
        output = Vanetza_ITS2_VruSubProfilePedestrian_first_responder;
    }
    else
    {
        error = "Unsupported VAM pedestrian sub-profile label: " + label;
        return false;
    }

    error.clear();
    return true;
}

bool ValidateInput(
    const VamInput& input,
    std::int64_t& referencePositionTimeMs,
    std::string& error)
{
    if (input.originatingStation.stationId.value == 0)
    {
        error = "VAM stationId must be non-zero";
        return false;
    }

    if (input.vruProfile != VruProfile::Pedestrian)
    {
        error = "This VAM UPER encoder supports Profile 1 (pedestrian) only";
        return false;
    }

    if (input.originatingStation.originatingItsSTypeLabel.has_value() &&
        *input.originatingStation.originatingItsSTypeLabel != "pedestrian")
    {
        error = "Profile 1 VAM originating ITS-S type must be pedestrian";
        return false;
    }

    if (input.clusterInformation.has_value() ||
        input.clusterOperation.has_value() ||
        input.motionPrediction.has_value())
    {
        error = "VAM clustering and motion-prediction containers are not supported by Profile 1 U1";
        return false;
    }

    std::int64_t generationTimeMs = 0;
    if (!ToTimestampItsMs(
            input.generationTimeIts,
            generationTimeMs,
            "VAM generationTimeIts",
            error))
    {
        return false;
    }

    if (!ToTimestampItsMs(
            input.referencePositionTimeIts,
            referencePositionTimeMs,
            "VAM referencePositionTimeIts",
            error))
    {
        return false;
    }

    if (referencePositionTimeMs > generationTimeMs)
    {
        error = "VAM reference position timestamp must not be newer than generation time";
        return false;
    }

    if (generationTimeMs - referencePositionTimeMs >=
        kMaxReferencePositionAgeMs)
    {
        error = "VAM reference position timestamp must be less than 32767 ms old";
        return false;
    }

    error.clear();
    return true;
}

} // namespace

VamUperEncodeResult VamUperEncoder::Encode(
    const VamInput& input,
    VamUperEncodeOptions options)
{
    std::int64_t referencePositionTimeMs = 0;
    std::string error;
    if (!ValidateInput(input, referencePositionTimeMs, error))
    {
        return Reject(error);
    }

    vanetza::asn1::r2::Vam message;
    auto* root = message.operator->();
    if (!root)
    {
        return Reject("Cannot allocate VAM R2 ASN.1 message");
    }

    root->header.protocolVersion = kVamProtocolVersion;
    root->header.messageId = Vanetza_ITS2_MessageId_vam;
    root->header.stationId =
        static_cast<long>(input.originatingStation.stationId.value);

    // GenerationDeltaTime is the ITS timestamp of the reference position,
    // in milliseconds since the ITS epoch, modulo 2^16.
    root->vam.generationDeltaTime = static_cast<long>(
        referencePositionTimeMs % kGenerationDeltaTimeModulus);

    auto& parameters = root->vam.vamParameters;
    parameters.basicContainer.stationType =
        Vanetza_ITS2_TrafficParticipantType_pedestrian;

    if (!SetReferencePosition(
            input,
            parameters.basicContainer.referencePosition,
            error))
    {
        return Reject(error);
    }

    auto& highFrequency = parameters.vruHighFrequencyContainer;
    if (!SetHeading(
            input.highFrequencyState.headingDeg,
            highFrequency.heading,
            error))
    {
        return Reject(error);
    }

    if (!SetSpeed(
            input.highFrequencyState.speedMps,
            highFrequency.speed,
            error))
    {
        return Reject(error);
    }

    if (!SetLongitudinalAcceleration(
            input.highFrequencyState.longitudinalAccelerationMps2,
            highFrequency.longitudinalAcceleration,
            error))
    {
        return Reject(error);
    }

    if (options.includeLowFrequencyContainer)
    {
        long subProfile = 0;
        if (!MapPedestrianSubProfile(input, subProfile, error))
        {
            return Reject(error);
        }

        auto* lowFrequency =
            static_cast<Vanetza_ITS2_VruLowFrequencyContainer_t*>(
                std::calloc(1, sizeof(Vanetza_ITS2_VruLowFrequencyContainer_t)));
        if (!lowFrequency)
        {
            return Reject("Cannot allocate VAM low-frequency container");
        }

        lowFrequency->profileAndSubprofile.present =
            Vanetza_ITS2_VruProfileAndSubprofile_PR_pedestrian;
        lowFrequency->profileAndSubprofile.choice.pedestrian = subProfile;
        parameters.vruLowFrequencyContainer = lowFrequency;
    }

    const auto headerValidation = VamHeaderValidator::Validate(root->header);
    if (!headerValidation.success)
    {
        return Reject(headerValidation.error);
    }

    char constraintBuffer[512] = {};
    std::size_t constraintLength = sizeof(constraintBuffer);
    if (asn_check_constraints(
            &asn_DEF_Vanetza_ITS2_VAM,
            root,
            constraintBuffer,
            &constraintLength) != 0)
    {
        return Reject(
            constraintBuffer[0] != '\0'
                ? constraintBuffer
                : "VAM ASN.1 constraints failed");
    }

    std::vector<std::uint8_t> buffer(kMaxEncodedBufferSize, 0);
    const asn_enc_rval_t encoded = uper_encode_to_buffer(
        &asn_DEF_Vanetza_ITS2_VAM,
        nullptr,
        root,
        buffer.data(),
        buffer.size());

    if (encoded.encoded <= 0)
    {
        return Reject(
            encoded.failed_type && encoded.failed_type->name
                ? std::string("VAM UPER encoding failed at ") +
                    encoded.failed_type->name
                : "VAM UPER encoding failed");
    }

    const auto encodedBits = static_cast<std::size_t>(encoded.encoded);
    const std::size_t encodedBytes = (encodedBits + 7U) / 8U;
    if (encodedBytes == 0 || encodedBytes > buffer.size())
    {
        return Reject("VAM UPER encoder returned an invalid encoded size");
    }

    VamUperEncodeResult result;
    result.success = true;
    result.encodedBits = encodedBits;
    result.bytes.assign(buffer.begin(), buffer.begin() + encodedBytes);
    result.error.clear();
    return result;
}

} // namespace vam
} // namespace vanetza_integration
