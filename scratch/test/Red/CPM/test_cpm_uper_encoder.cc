#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"
#include "vanetza_integration/cpm/types/cpm_input.h"

#include <vanetza/asn1/cpm.hpp>
#include <vanetza/asn1/support/INTEGER.h>
#include <vanetza/asn1/its/r2/CpmContainerId.h>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/AltitudeValue.h>
#include <vanetza/asn1/its/r2/AltitudeConfidence.h>
#include <vanetza/asn1/its/r2/SemiAxisLength.h>
#include <vanetza/asn1/its/r2/HeadingValue.h>
#include <vanetza/asn1/its/r2/CoordinateConfidence.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <variant>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    return condition;
}

CpmInput MakeValidInput()
{
    CpmInput input{};
    input.station.stationId = 9001;
    input.station.origin = CpmRoadSideUnitStationData{};
    input.referenceTimeItsMs = 123456;
    input.referencePosition.latitudeDeg = 41.42528995;
    input.referencePosition.longitudeDeg = -8.43407620;
    input.referencePosition.altitudeMeters = std::nullopt;
    input.knownObjectCount = 1;

    CpmPerceivedObject object{};
    object.objectId = 42;
    object.measurementDeltaTimeMs = -12;
    object.position.xEast = {5.12, std::nullopt};
    object.position.yNorth = {-3.24, std::nullopt};
    object.position.zUp = std::nullopt;
    object.objectAgeMs = 1500;

    input.perceivedObjects.push_back(object);
    return input;
}

bool CheckRoundTrip(const CpmUperEncodeResult& encoded)
{
    if (!encoded.success || encoded.bytes.empty() ||
        encoded.encodedBits == 0 ||
        encoded.encodedBits > encoded.bytes.size() * 8 ||
        encoded.encodedBits <= (encoded.bytes.size() - 1) * 8)
    {
        return Check(false, "ENCODED_RESULT_HAS_VALID_SIZE");
    }

    if (!Check(true, "ENCODED_RESULT_HAS_VALID_SIZE"))
    {
        return false;
    }

    vanetza::asn1::r2::Cpm decoded;
    if (!decoded.decode(encoded.bytes.data(), encoded.bytes.size()))
    {
        return Check(false, "UPER_ROUNDTRIP_DECODE");
    }

    if (!Check(true, "UPER_ROUNDTRIP_DECODE"))
    {
        return false;
    }

    const auto* root = decoded.operator->();
    bool passed = true;

    passed = Check(
        root->header.protocolVersion == 2 &&
        root->header.messageId == Vanetza_ITS2_MessageId_cpm &&
        root->header.stationId == 9001,
        "RSU_HEADER_PRESERVED") && passed;

    long referenceTime = -1;
    const bool referenceTimeDecoded =
        asn_INTEGER2long(
            &root->payload.managementContainer.referenceTime,
            &referenceTime) == 0;

    passed = Check(
        referenceTimeDecoded && referenceTime == 123456,
        "REFERENCE_TIME_PRESERVED") && passed;

    const auto& reference =
        root->payload.managementContainer.referencePosition;
    const auto& ellipse = reference.positionConfidenceEllipse;
    const auto& altitude = reference.altitude;

    passed = Check(
        reference.latitude == 414252900 &&
        reference.longitude == -84340762,
        "WGS84_REFERENCE_POSITION_QUANTIZED") && passed;

    passed = Check(
        altitude.altitudeValue ==
            Vanetza_ITS2_AltitudeValue_unavailable &&
        altitude.altitudeConfidence ==
            Vanetza_ITS2_AltitudeConfidence_unavailable,
        "UNKNOWN_ALTITUDE_PRESERVED") && passed;

    passed = Check(
        ellipse.semiMajorConfidence ==
            Vanetza_ITS2_SemiAxisLength_unavailable &&
        ellipse.semiMinorConfidence ==
            Vanetza_ITS2_SemiAxisLength_unavailable &&
        ellipse.semiMajorOrientation ==
            Vanetza_ITS2_HeadingValue_unavailable,
        "UNKNOWN_REFERENCE_CONFIDENCE_PRESERVED") && passed;

    const auto& containers = root->payload.cpmContainers.list;
    if (containers.count != 2 || !containers.array ||
        !containers.array[0] || !containers.array[1])
    {
        return Check(false, "RSU_AND_OBJECT_CONTAINERS_PRESENT") && passed;
    }

    const auto* rsu = containers.array[0];
    const auto* objectWrapper = containers.array[1];

    passed = Check(
        rsu->containerId ==
            Vanetza_ITS2_CpmContainerId_originatingRsuContainer &&
        rsu->containerData.present ==
            Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingRsuContainer,
        "RSU_ORIGIN_CONTAINER_PRESENT") && passed;

    if (objectWrapper->containerData.present !=
        Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer)
    {
        return Check(false, "PERCEIVED_OBJECT_CONTAINER_PRESENT") && passed;
    }

    const auto& objectContainer =
        objectWrapper->containerData.choice.PerceivedObjectContainer;
    const auto& objects = objectContainer.perceivedObjects.list;

    passed = Check(
        objectWrapper->containerId ==
            Vanetza_ITS2_CpmContainerId_perceivedObjectContainer &&
        objectContainer.numberOfPerceivedObjects == 1 &&
        objects.count == 1 && objects.array && objects.array[0],
        "PERCEIVED_OBJECT_CONTAINER_PRESENT") && passed;

    if (objects.count != 1 || !objects.array || !objects.array[0])
    {
        return false;
    }

    const auto* object = objects.array[0];

    passed = Check(
        object->objectId && *object->objectId == 42 &&
        object->measurementDeltaTime == -12 &&
        object->objectAge && *object->objectAge == 1500,
        "OBJECT_ID_TIME_DELTA_AND_AGE_PRESERVED") && passed;

    passed = Check(
        object->position.xCoordinate.value == 512 &&
        object->position.yCoordinate.value == -324 &&
        object->position.xCoordinate.confidence ==
            Vanetza_ITS2_CoordinateConfidence_unavailable &&
        object->position.yCoordinate.confidence ==
            Vanetza_ITS2_CoordinateConfidence_unavailable &&
        object->position.zCoordinate == nullptr,
        "ENU_POSITION_AND_UNKNOWN_CONFIDENCE_PRESERVED") && passed;

    return passed;
}

} // namespace

int main()
{
    bool passed = true;

    const auto input = MakeValidInput();
    const auto encoded = CpmUperEncoder::Encode(input);

    passed = Check(
        encoded.success && encoded.error.empty(),
        "VALID_RSU_INPUT_ENCODED") && passed;

    if (encoded.success)
    {
        passed = CheckRoundTrip(encoded) && passed;
    }
    else
    {
        std::cerr << "ENCODE_ERROR=" << encoded.error << '\n';
    }

    auto invalidAge = MakeValidInput();
    invalidAge.perceivedObjects.front().objectAgeMs = 1501;
    const auto badAgeResult = CpmUperEncoder::Encode(invalidAge);

    passed = Check(
        !badAgeResult.success && badAgeResult.bytes.empty() &&
        !badAgeResult.error.empty(),
        "OBJECT_AGE_ABOVE_1500_REJECTED") && passed;

    auto invalidStation = MakeValidInput();
    invalidStation.station.stationId = 0;
    const auto badStationResult = CpmUperEncoder::Encode(invalidStation);

    passed = Check(
        !badStationResult.success && badStationResult.bytes.empty() &&
        !badStationResult.error.empty(),
        "ZERO_STATION_ID_REJECTED") && passed;

    auto invalidPosition = MakeValidInput();
    invalidPosition.perceivedObjects.front().position.xEast.value =
        std::numeric_limits<double>::quiet_NaN();
    const auto badPositionResult = CpmUperEncoder::Encode(invalidPosition);

    passed = Check(
        !badPositionResult.success && badPositionResult.bytes.empty() &&
        !badPositionResult.error.empty(),
        "NONFINITE_POSITION_REJECTED") && passed;

    if (!passed)
    {
        return 1;
    }

    std::cout << "CPM_UPER_ENCODER_CONTRACT=PASS\n";
    return 0;
}
