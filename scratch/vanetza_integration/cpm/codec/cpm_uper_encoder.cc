#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"

#include "vanetza_integration/cpm/perception/cpm_coordinate_quantizer.h"

#include <vanetza/asn1/cpm.hpp>
#include <vanetza/asn1/support/INTEGER.h>
#include <vanetza/asn1/support/asn_SEQUENCE_OF.h>
#include <vanetza/asn1/its/r2/AltitudeConfidence.h>
#include <vanetza/asn1/its/r2/AltitudeValue.h>
#include <vanetza/asn1/its/r2/CpmContainerId.h>
#include <vanetza/asn1/its/r2/CoordinateConfidence.h>
#include <vanetza/asn1/its/r2/HeadingValue.h>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/SemiAxisLength.h>

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

extern "C"
{
#include "asn_application.h"
#include "constr_TYPE.h"
}

namespace
{

constexpr std::int64_t kMaxTimestampItsMs = 4398046511103LL;
constexpr std::size_t kMaxEncodedBufferSize = 65536;

// Limite operacional local do MVP para um único payload CPM UPER.
// Não representa, por si só, um limite normativo ETSI ou um MTU.
constexpr std::size_t kLocalMaxEncodedPayloadBytes = 1200;
constexpr long kGeodeticScale = 10000000L;

CpmUperEncodeResult Reject(std::string error)
{
    CpmUperEncodeResult result;
    result.error = std::move(error);
    return result;
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

    // -180 degrees is the reserved Longitude valueNotUsed code in this CDD.
    if (longitude && degrees == -180.0)
    {
        error = "Longitude -180 degrees maps to the reserved valueNotUsed code";
        return false;
    }

    const long double scaled =
        static_cast<long double>(degrees) * kGeodeticScale;

    // A entrada é double. Perto de uma fronteira de meio inteiro,
    // a representação binária pode deslocar ligeiramente um decimal
    // exato para baixo ou para cima. Estabilizar apenas dentro da
    // incerteza numérica da entrada antes de arredondar.
    const long double nearestHalf =
        std::floor(scaled) + 0.5L;
    const long double magnitude =
        std::max(1.0L, std::abs(scaled));
    const long double halfBoundaryTolerance =
        0.5L *
        static_cast<long double>(
            std::numeric_limits<double>::epsilon()) *
        magnitude;

    const long double stableScaled =
        std::abs(scaled - nearestHalf) <= halfBoundaryTolerance
            ? nearestHalf
            : scaled;
    const long double rounded = std::round(stableScaled);

    if (rounded <
            static_cast<long double>(std::numeric_limits<long>::min()) ||
        rounded >
            static_cast<long double>(std::numeric_limits<long>::max()))
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

bool HasUnsupportedInput(const CpmInput& input, std::string& error)
{
    const auto* rsu =
        std::get_if<CpmRoadSideUnitStationData>(&input.station.origin);

    if (!rsu)
    {
        error = "This CPM UPER encoder supports RSU-origin messages only";
        return true;
    }

    if (rsu->mapReference.has_value())
    {
        error = "RSU mapReference mapping is not implemented";
        return true;
    }

    const auto& reference = input.referencePosition;
    if (reference.altitudeMeters.has_value() ||
        reference.altitudeConfidenceMeters.state !=
            CpmAltitudeAccuracyState::Unavailable ||
        reference.confidenceEllipse.semiMajorConfidenceMeters.state !=
            CpmSemiAxisState::Unavailable ||
        reference.confidenceEllipse.semiMinorConfidenceMeters.state !=
            CpmSemiAxisState::Unavailable ||
        reference.confidenceEllipse.semiMajorOrientationDeg.state !=
            CpmHeadingState::Unavailable)
    {
        error =
            "Known reference altitude or confidence mapping is not implemented";
        return true;
    }

    if (input.segmentationInfo.has_value() ||
        input.messageRateRange.has_value())
    {
        error = "CPM segmentation and message-rate metadata are not implemented";
        return true;
    }

    if (!input.sensorInformation.empty() ||
        !input.perceptionRegions.empty())
    {
        error =
            "Sensor-information and perception-region containers are not implemented";
        return true;
    }

    for (const auto& object : input.perceivedObjects)
    {
        if (object.position.zUp.has_value() ||
            object.velocity.has_value() ||
            object.acceleration.has_value() ||
            object.angles.has_value() ||
            object.angularVelocity.has_value() ||
            object.correlationMatrices.has_value() ||
            object.dimensionX.has_value() ||
            object.dimensionY.has_value() ||
            object.dimensionZ.has_value() ||
            object.objectPerceptionQuality.has_value() ||
            object.mapPosition.has_value() ||
            object.sensorIds.has_value() ||
            object.classifications.has_value())
        {
            error =
                "Perceived-object fields outside the horizontal-position profile are not implemented";
            return true;
        }

        if (object.position.xEast.confidence.has_value() ||
            object.position.yNorth.confidence.has_value())
        {
            error =
                "Known Cartesian coordinate-confidence mapping is not implemented";
            return true;
        }
    }

    error.clear();
    return false;
}

void FreeUnattachedObject(Vanetza_ITS2_PerceivedObject_t* object)
{
    if (!object)
    {
        return;
    }

    std::free(object->objectId);
    std::free(object->objectAge);
    std::free(object);
}

bool SetReferencePosition(
    const CpmInput& input,
    Vanetza_ITS2_ReferencePosition_t& output,
    std::string& error)
{
    if (!input.referencePosition.latitudeDeg.has_value() ||
        !input.referencePosition.longitudeDeg.has_value())
    {
        error = "RSU latitude and longitude are required for CPM encoding";
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
        error = "Invalid RSU latitude: " + error;
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
        error = "Invalid RSU longitude: " + error;
        return false;
    }

    output.latitude = static_cast<Vanetza_ITS2_Latitude_t>(latitude);
    output.longitude = static_cast<Vanetza_ITS2_Longitude_t>(longitude);

    output.positionConfidenceEllipse.semiMajorConfidence =
        Vanetza_ITS2_SemiAxisLength_unavailable;
    output.positionConfidenceEllipse.semiMinorConfidence =
        Vanetza_ITS2_SemiAxisLength_unavailable;
    output.positionConfidenceEllipse.semiMajorOrientation =
        Vanetza_ITS2_HeadingValue_unavailable;

    output.altitude.altitudeValue =
        Vanetza_ITS2_AltitudeValue_unavailable;
    output.altitude.altitudeConfidence =
        Vanetza_ITS2_AltitudeConfidence_unavailable;

    error.clear();
    return true;
}

bool AddRsuOriginContainer(
    Vanetza_ITS2_CollectivePerceptionMessage_t& root,
    std::string& error)
{
    auto* wrapper =
        static_cast<Vanetza_ITS2_WrappedCpmContainer_t*>(
            std::calloc(1, sizeof(Vanetza_ITS2_WrappedCpmContainer_t)));

    if (!wrapper)
    {
        error = "Cannot allocate RSU-origin CPM container";
        return false;
    }

    wrapper->containerId =
        Vanetza_ITS2_CpmContainerId_originatingRsuContainer;
    wrapper->containerData.present =
        Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingRsuContainer;

    if (ASN_SEQUENCE_ADD(&root.payload.cpmContainers.list, wrapper) != 0)
    {
        std::free(wrapper);
        error = "Cannot append RSU-origin CPM container";
        return false;
    }

    error.clear();
    return true;
}

bool AddPerceivedObjectsContainer(
    Vanetza_ITS2_CollectivePerceptionMessage_t& root,
    const CpmInput& input,
    std::string& error)
{
    auto* wrapper =
        static_cast<Vanetza_ITS2_WrappedCpmContainer_t*>(
            std::calloc(1, sizeof(Vanetza_ITS2_WrappedCpmContainer_t)));

    if (!wrapper)
    {
        error = "Cannot allocate perceived-object CPM container";
        return false;
    }

    wrapper->containerId =
        Vanetza_ITS2_CpmContainerId_perceivedObjectContainer;
    wrapper->containerData.present =
        Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer;

    if (ASN_SEQUENCE_ADD(&root.payload.cpmContainers.list, wrapper) != 0)
    {
        std::free(wrapper);
        error = "Cannot append perceived-object CPM container";
        return false;
    }

    auto& container =
        wrapper->containerData.choice.PerceivedObjectContainer;
    container.numberOfPerceivedObjects =
        static_cast<Vanetza_ITS2_CardinalNumber1B_t>(
            input.knownObjectCount);

    for (const auto& semanticObject : input.perceivedObjects)
    {
        if (semanticObject.objectId == 0)
        {
            error = "Perceived-object identifier must be non-zero";
            return false;
        }

        if (semanticObject.measurementDeltaTimeMs < -2048 ||
            semanticObject.measurementDeltaTimeMs > 2047)
        {
            error = "measurementDeltaTime must be within -2048..2047";
            return false;
        }

        if (semanticObject.objectAgeMs.has_value() &&
            (*semanticObject.objectAgeMs < 0 ||
             *semanticObject.objectAgeMs > 1500))
        {
            error = "ObjectAge must be within the CPM R2 range 0..1500";
            return false;
        }

        std::int32_t x = 0;
        std::string quantizerError;
        if (!CpmCoordinateQuantizer::ToAsn1Value(
                semanticObject.position.xEast.value, x, quantizerError))
        {
            error = "Invalid east coordinate: " + quantizerError;
            return false;
        }

        std::int32_t y = 0;
        quantizerError.clear();
        if (!CpmCoordinateQuantizer::ToAsn1Value(
                semanticObject.position.yNorth.value, y, quantizerError))
        {
            error = "Invalid north coordinate: " + quantizerError;
            return false;
        }

        auto* object =
            static_cast<Vanetza_ITS2_PerceivedObject_t*>(
                std::calloc(1, sizeof(Vanetza_ITS2_PerceivedObject_t)));

        if (!object)
        {
            error = "Cannot allocate perceived object";
            return false;
        }

        object->objectId =
            static_cast<Vanetza_ITS2_Identifier2B_t*>(
                std::calloc(1, sizeof(Vanetza_ITS2_Identifier2B_t)));

        if (!object->objectId)
        {
            std::free(object);
            error = "Cannot allocate perceived-object identifier";
            return false;
        }

        *object->objectId = semanticObject.objectId;

        if (semanticObject.objectAgeMs.has_value())
        {
            object->objectAge =
                static_cast<Vanetza_ITS2_DeltaTimeMilliSecondSigned_t*>(
                    std::calloc(
                        1,
                        sizeof(Vanetza_ITS2_DeltaTimeMilliSecondSigned_t)));

            if (!object->objectAge)
            {
                FreeUnattachedObject(object);
                error = "Cannot allocate perceived-object age";
                return false;
            }

            *object->objectAge = *semanticObject.objectAgeMs;
        }

        object->measurementDeltaTime =
            semanticObject.measurementDeltaTimeMs;

        object->position.xCoordinate.value =
            static_cast<Vanetza_ITS2_CartesianCoordinateLarge_t>(x);
        object->position.yCoordinate.value =
            static_cast<Vanetza_ITS2_CartesianCoordinateLarge_t>(y);
        object->position.xCoordinate.confidence =
            Vanetza_ITS2_CoordinateConfidence_unavailable;
        object->position.yCoordinate.confidence =
            Vanetza_ITS2_CoordinateConfidence_unavailable;

        if (ASN_SEQUENCE_ADD(
                &container.perceivedObjects.list, object) != 0)
        {
            FreeUnattachedObject(object);
            error = "Cannot append perceived object to CPM container";
            return false;
        }
    }

    error.clear();
    return true;
}

} // namespace

CpmUperEncodeResult
CpmUperEncoder::Encode(const CpmInput& input)
{
    if (input.station.stationId == 0)
    {
        return Reject("CPM stationId must be non-zero");
    }

    if (input.referenceTimeItsMs < 0 ||
        input.referenceTimeItsMs > kMaxTimestampItsMs)
    {
        return Reject("CPM referenceTime is outside the TimestampIts range");
    }

    if (input.perceivedObjects.empty() ||
        input.perceivedObjects.size() > 255)
    {
        return Reject("This CPM encoder requires 1..255 perceived objects");
    }

    if (input.knownObjectCount < input.perceivedObjects.size())
    {
        return Reject(
            "knownObjectCount is below the number of included objects");
    }

    // O mesmo objeto não pode ser incluído mais do que uma vez
    // na mensagem CPM produzida por este codificador.
    std::unordered_set<std::uint16_t> seenObjectIds;
    for (const auto& object : input.perceivedObjects)
    {
        if (!seenObjectIds.insert(object.objectId).second)
        {
            return Reject(
                "CPM perceived-object objectId values must be unique");
        }
    }

    std::string error;
    if (HasUnsupportedInput(input, error))
    {
        return Reject(error);
    }

    vanetza::asn1::r2::Cpm message;
    auto* root = message.operator->();

    if (!root)
    {
        return Reject("Cannot allocate CPM R2 ASN.1 message");
    }

    root->header.protocolVersion = 2;
    root->header.messageId = Vanetza_ITS2_MessageId_cpm;
    root->header.stationId = input.station.stationId;

    if (asn_long2INTEGER(
            &root->payload.managementContainer.referenceTime,
            static_cast<long>(input.referenceTimeItsMs)) != 0)
    {
        return Reject("Cannot encode CPM referenceTime as INTEGER");
    }

    if (!SetReferencePosition(
            input,
            root->payload.managementContainer.referencePosition,
            error))
    {
        return Reject(error);
    }

    if (!AddRsuOriginContainer(*root, error))
    {
        return Reject(error);
    }

    if (!AddPerceivedObjectsContainer(*root, input, error))
    {
        return Reject(error);
    }

    char constraintBuffer[512] = {};
    std::size_t constraintLength = sizeof(constraintBuffer);

    if (asn_check_constraints(
            &asn_DEF_Vanetza_ITS2_CollectivePerceptionMessage,
            root,
            constraintBuffer,
            &constraintLength) != 0)
    {
        return Reject(
            constraintBuffer[0] != '\0'
                ? constraintBuffer
                : "CPM ASN.1 constraints failed");
    }

    std::vector<std::uint8_t> buffer(kMaxEncodedBufferSize, 0);

    const asn_enc_rval_t encoded = uper_encode_to_buffer(
        &asn_DEF_Vanetza_ITS2_CollectivePerceptionMessage,
        nullptr,
        root,
        buffer.data(),
        buffer.size());

    if (encoded.encoded <= 0)
    {
        return Reject(
            encoded.failed_type && encoded.failed_type->name
                ? std::string("UPER encoding failed at ") +
                    encoded.failed_type->name
                : "CPM UPER encoding failed");
    }

    const auto encodedBits =
        static_cast<std::size_t>(encoded.encoded);
    const std::size_t encodedBytes = (encodedBits + 7) / 8;

    if (encodedBytes == 0 || encodedBytes > buffer.size())
    {
        return Reject("CPM UPER encoder returned an invalid encoded size");
    }

    // Aplicar o limite ao tamanho UPER efetivo, não à estimativa
    // usada anteriormente pelo CpmGenerationManager.
    if (encodedBytes > kLocalMaxEncodedPayloadBytes)
    {
        return Reject(
            "CPM UPER payload exceeds local maximum of 1200 bytes");
    }

    CpmUperEncodeResult result;
    result.success = true;
    result.encodedBits = encodedBits;
    result.bytes.assign(buffer.begin(), buffer.begin() + encodedBytes);
    result.error.clear();
    return result;
}
