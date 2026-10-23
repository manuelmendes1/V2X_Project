#include "vanetza_integration/cpm/codec/cpm_uper_decoder.h"

#include "vanetza_integration/cpm/perception/cpm_coordinate_quantizer.h"

#include <vanetza/asn1/support/INTEGER.h>
#include <vanetza/asn1/its/r2/CollectivePerceptionMessage.h>
#include <vanetza/asn1/its/r2/PerceivedObject.h>
#include <vanetza/asn1/its/r2/PerceivedObjectContainer.h>
#include <vanetza/asn1/its/r2/ReferencePosition.h>

extern "C"
{
#include <vanetza/asn1/support/asn_application.h>
#include <vanetza/asn1/support/asn_internal.h>
#include <vanetza/asn1/support/constr_TYPE.h>
}
#include <vanetza/asn1/its/r2/AltitudeConfidence.h>
#include <vanetza/asn1/its/r2/AltitudeValue.h>
#include <vanetza/asn1/its/r2/CartesianCoordinateLarge.h>
#include <vanetza/asn1/its/r2/CoordinateConfidence.h>
#include <vanetza/asn1/its/r2/CpmContainerId.h>
#include <vanetza/asn1/its/r2/HeadingValue.h>
#include <vanetza/asn1/its/r2/Latitude.h>
#include <vanetza/asn1/its/r2/Longitude.h>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/SemiAxisLength.h>
#include <vanetza/asn1/its/r2/WrappedCpmContainer.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace
{

using AsnRoot = Vanetza_ITS2_CollectivePerceptionMessage_t;
using AsnReferencePosition = Vanetza_ITS2_ReferencePosition_t;
using AsnPerceivedObjectContainer =
    Vanetza_ITS2_PerceivedObjectContainer_t;
using AsnPerceivedObject = Vanetza_ITS2_PerceivedObject_t;

void FreeAsnRoot(AsnRoot* root)
{
    if (root)
    {
        ASN_STRUCT_FREE(
            asn_DEF_Vanetza_ITS2_CollectivePerceptionMessage,
            root);
    }
}

CpmUperDecodeResult Reject(std::string error)
{
    CpmUperDecodeResult result;
    result.error = std::move(error);
    return result;
}

bool DecodeReferencePosition(
    const AsnReferencePosition& source,
    CpmReferencePosition& target,
    std::string& error)
{
    if (source.latitude == Vanetza_ITS2_Latitude_unavailable)
    {
        target.latitudeDeg.reset();
    }
    else
    {
        if (source.latitude < -900000000L ||
            source.latitude > 900000000L)
        {
            error = "CPM latitude uses an unsupported reserved value";
            return false;
        }

        target.latitudeDeg =
            static_cast<double>(source.latitude) / 10000000.0;
    }

    if (source.longitude == Vanetza_ITS2_Longitude_unavailable)
    {
        target.longitudeDeg.reset();
    }
    else
    {
        if (source.longitude == Vanetza_ITS2_Longitude_valueNotUsed)
        {
            error = "CPM longitude uses valueNotUsed, which the domain model cannot distinguish";
            return false;
        }

        if (source.longitude < -1800000000L ||
            source.longitude > 1800000000L)
        {
            error = "CPM longitude is outside the supported WGS84 range";
            return false;
        }

        target.longitudeDeg =
            static_cast<double>(source.longitude) / 10000000.0;
    }

    // The current CPM R2 encoder profile emits the unavailable code for
    // altitude and all reference-position confidence components.
    // Reject other values until a lossless inverse mapper is implemented.
    if (source.altitude.altitudeValue !=
            Vanetza_ITS2_AltitudeValue_unavailable ||
        source.altitude.altitudeConfidence !=
            Vanetza_ITS2_AltitudeConfidence_unavailable)
    {
        error = "CPM reference altitude/confidence is outside the supported profile";
        return false;
    }

    const auto& ellipse = source.positionConfidenceEllipse;
    if (ellipse.semiMajorConfidence !=
            Vanetza_ITS2_SemiAxisLength_unavailable ||
        ellipse.semiMinorConfidence !=
            Vanetza_ITS2_SemiAxisLength_unavailable ||
        ellipse.semiMajorOrientation !=
            Vanetza_ITS2_HeadingValue_unavailable)
    {
        error = "CPM reference confidence ellipse is outside the supported profile";
        return false;
    }

    target.altitudeMeters.reset();
    target.altitudeConfidenceMeters =
        CpmAltitudeAccuracy{CpmAltitudeAccuracyState::Unavailable, 0.0};
    target.confidenceEllipse.semiMajorConfidenceMeters =
        CpmSemiAxisLength{CpmSemiAxisState::Unavailable, 0.0};
    target.confidenceEllipse.semiMinorConfidenceMeters =
        CpmSemiAxisLength{CpmSemiAxisState::Unavailable, 0.0};
    target.confidenceEllipse.semiMajorOrientationDeg =
        CpmHeadingValue{CpmHeadingState::Unavailable, 0.0};

    error.clear();
    return true;
}

bool DecodePerceivedObjects(
    const AsnPerceivedObjectContainer& source,
    CpmMessage& target,
    std::string& error)
{
    const long declaredCount =
        static_cast<long>(source.numberOfPerceivedObjects);
    const auto& objects = source.perceivedObjects.list;

    if (declaredCount < 0 || declaredCount > 255)
    {
        error = "CPM numberOfPerceivedObjects must be within 0..255";
        return false;
    }

    if (objects.count < 0 ||
        objects.count > 255 ||
        (objects.count > 0 && objects.array == nullptr))
    {
        error = "CPM perceived-object list cardinality is invalid";
        return false;
    }

    if (declaredCount < objects.count)
    {
        error = "CPM numberOfPerceivedObjects is below the included list size";
        return false;
    }

    // This is the sender's total known-object count at message
    // generation. It can exceed the number of objects included below.
    target.knownObjectCount = static_cast<std::uint8_t>(declaredCount);

    for (int index = 0; index < objects.count; ++index)
    {
        const auto* object = objects.array[index];

        if (!object)
        {
            error = "CPM perceived-object list contains a null entry";
            return false;
        }

        if (!object->objectId || *object->objectId == 0)
        {
            error = "CPM perceived object requires a non-zero objectId";
            return false;
        }

        if (object->measurementDeltaTime < -2048 ||
            object->measurementDeltaTime > 2047)
        {
            error = "CPM measurementDeltaTime is outside -2048..2047";
            return false;
        }

        if (object->objectAge &&
            (*object->objectAge < 0 || *object->objectAge > 1500))
        {
            error = "CPM objectAge is outside the supported range 0..1500";
            return false;
        }

        // The current encoder supports horizontal Cartesian coordinates,
        // unknown coordinate confidence, optional objectAge, and no other
        // PerceivedObject optionals.
        if (object->position.zCoordinate ||
            object->velocity ||
            object->acceleration ||
            object->angles ||
            object->zAngularVelocity ||
            object->lowerTriangularCorrelationMatrices ||
            object->objectDimensionX ||
            object->objectDimensionY ||
            object->objectDimensionZ ||
            object->objectPerceptionQuality ||
            object->sensorIdList ||
            object->classification ||
            object->mapPosition)
        {
            error = "CPM perceived-object optional field is outside the supported profile";
            return false;
        }

        const auto& x = object->position.xCoordinate;
        const auto& y = object->position.yCoordinate;

        if (x.confidence !=
                Vanetza_ITS2_CoordinateConfidence_unavailable ||
            y.confidence !=
                Vanetza_ITS2_CoordinateConfidence_unavailable)
        {
            error = "CPM coordinate confidence is outside the supported profile";
            return false;
        }

        if (x.value < CpmCoordinateQuantizer::kMinNormalValue ||
            x.value > CpmCoordinateQuantizer::kMaxNormalValue ||
            y.value < CpmCoordinateQuantizer::kMinNormalValue ||
            y.value > CpmCoordinateQuantizer::kMaxNormalValue)
        {
            error = "CPM Cartesian coordinate uses an out-of-range reserved code";
            return false;
        }

        CpmPerceivedObject decoded{};
        decoded.objectId =
            static_cast<std::uint16_t>(*object->objectId);
        decoded.measurementDeltaTimeMs =
            static_cast<std::int32_t>(object->measurementDeltaTime);

        decoded.position.xEast.value =
            static_cast<double>(x.value) / 100.0;
        decoded.position.yNorth.value =
            static_cast<double>(y.value) / 100.0;
        decoded.position.xEast.confidence.reset();
        decoded.position.yNorth.confidence.reset();

        if (object->objectAge)
        {
            decoded.objectAgeMs =
                static_cast<std::int32_t>(*object->objectAge);
        }

        target.perceivedObjects.push_back(std::move(decoded));
    }

    error.clear();
    return true;
}

} // namespace

CpmUperDecodeResult
CpmUperDecoder::Decode(
    const std::vector<std::uint8_t>& payload,
    std::int64_t receivedAtUnixMs)
{
    if (payload.empty())
    {
        return Reject("empty payload");
    }

    AsnRoot* rawRoot = nullptr;
    const asn_dec_rval_t decodeResult = uper_decode(
        nullptr,
        &asn_DEF_Vanetza_ITS2_CollectivePerceptionMessage,
        reinterpret_cast<void**>(&rawRoot),
        payload.data(),
        payload.size(),
        0,
        0);

    std::unique_ptr<AsnRoot, decltype(&FreeAsnRoot)> decoded(
        rawRoot,
        &FreeAsnRoot);

    if (decodeResult.code != RC_OK || !decoded)
    {
        return Reject("ASN.1 decode failed");
    }

    const std::size_t consumedBits =
        static_cast<std::size_t>(decodeResult.consumed);
    const std::size_t consumedBytes = (consumedBits + 7U) / 8U;

    if (consumedBytes != payload.size())
    {
        return Reject("trailing bytes after CPM UPER payload");
    }

    // UPER occupies complete octets on the wire. Bits not used by the
    // final ASN.1 value are padding and must be zero.
    const unsigned usedBitsInLastOctet =
        static_cast<unsigned>(consumedBits % 8U);

    if (usedBitsInLastOctet != 0U)
    {
        const unsigned paddingBits = 8U - usedBitsInLastOctet;
        const auto paddingMask =
            static_cast<std::uint8_t>((1U << paddingBits) - 1U);

        if ((payload.back() & paddingMask) != 0U)
        {
            return Reject("non-zero UPER padding bits");
        }
    }

    char constraintBuffer[512] = {};
    std::size_t constraintLength = sizeof(constraintBuffer);

    if (asn_check_constraints(
            &asn_DEF_Vanetza_ITS2_CollectivePerceptionMessage,
            decoded.get(),
            constraintBuffer,
            &constraintLength) != 0)
    {
        return Reject(
            constraintBuffer[0] != '\0'
                ? std::string("CPM ASN.1 constraints failed: ") +
                    constraintBuffer
                : "CPM ASN.1 constraints failed");
    }

    const auto* root = decoded.get();
    if (!root)
    {
        return Reject("decoded CPM has no ASN.1 root");
    }

    if (root->header.protocolVersion != 2)
    {
        return Reject("unsupported CPM protocolVersion");
    }

    if (root->header.messageId != Vanetza_ITS2_MessageId_cpm)
    {
        return Reject("unsupported CPM messageId");
    }

    if (root->header.stationId <= 0 ||
        static_cast<unsigned long long>(root->header.stationId) >
            std::numeric_limits<std::uint32_t>::max())
    {
        return Reject("CPM stationId is outside the supported range");
    }

    const auto& management = root->payload.managementContainer;
    if (management.segmentationInfo || management.messageRateRange)
    {
        return Reject("CPM management metadata is outside the supported profile");
    }

    CpmMessage message{};
    message.station.stationId =
        static_cast<std::uint32_t>(root->header.stationId);
    message.station.origin = CpmRoadSideUnitStationData{};
    message.receivedAtUnixMs = receivedAtUnixMs;

    long referenceTime = 0;
    if (asn_INTEGER2long(
            &management.referenceTime,
            &referenceTime) != 0 ||
        referenceTime < 0)
    {
        return Reject("CPM referenceTime cannot be represented as a non-negative integer");
    }

    message.referenceTimeItsMs =
        static_cast<std::int64_t>(referenceTime);

    std::string error;
    if (!DecodeReferencePosition(
            management.referencePosition,
            message.referencePosition,
            error))
    {
        return Reject(error);
    }

    const auto& containers = root->payload.cpmContainers.list;
    if (containers.count < 1 ||
        containers.count > 8 ||
        containers.array == nullptr)
    {
        return Reject("CPM container list cardinality is invalid");
    }

    bool hasRsuContainer = false;
    bool hasPerceivedObjectContainer = false;

    for (int index = 0; index < containers.count; ++index)
    {
        const auto* container = containers.array[index];
        if (!container)
        {
            return Reject("CPM container list contains a null entry");
        }

        if (container->containerId ==
            Vanetza_ITS2_CpmContainerId_originatingRsuContainer)
        {
            if (hasRsuContainer ||
                container->containerData.present !=
                    Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingRsuContainer)
            {
                return Reject("CPM RSU container is duplicated or has an inconsistent CHOICE");
            }

            if (container->containerData.choice.OriginatingRsuContainer.mapReference)
            {
                return Reject("CPM RSU mapReference is outside the supported profile");
            }

            hasRsuContainer = true;
        }
        else if (container->containerId ==
                 Vanetza_ITS2_CpmContainerId_perceivedObjectContainer)
        {
            if (hasPerceivedObjectContainer ||
                container->containerData.present !=
                    Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer)
            {
                return Reject("CPM perceived-object container is duplicated or has an inconsistent CHOICE");
            }

            if (!DecodePerceivedObjects(
                    container->containerData.choice.PerceivedObjectContainer,
                    message,
                    error))
            {
                return Reject(error);
            }

            hasPerceivedObjectContainer = true;
        }
        else
        {
            return Reject("CPM container is outside the supported RSU profile");
        }
    }

    if (!hasRsuContainer || !hasPerceivedObjectContainer)
    {
        return Reject("CPM is missing a required profile container");
    }

    CpmUperDecodeResult result;
    result.success = true;
    result.message.emplace(std::move(message));
    result.error.clear();
    return result;
}
