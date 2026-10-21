#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "vanetza_integration/cpm/types/cpm_input.h"
#include "vanetza_integration/cpm/types/cpm_message.h"
#include "vanetza_integration/cpm/validation/cpm_management_metadata_validation.h"
#include "vanetza_integration/cpm/validation/cpm_confidence_ratio_validation.h"

#include <utility>
#include <limits>

// Deteção de campos que o perfil CPM R2 proíbe em TrailerData.
template <typename T, typename = void>
struct HasCpmTrailerFrontOverhang : std::false_type {};

template <typename T>
struct HasCpmTrailerFrontOverhang<
    T, std::void_t<decltype(std::declval<T>().frontOverhangMeters)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasCpmTrailerRearOverhang : std::false_type {};

template <typename T>
struct HasCpmTrailerRearOverhang<
    T, std::void_t<decltype(std::declval<T>().rearOverhangMeters)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasCpmTrailerWidth : std::false_type {};

template <typename T>
struct HasCpmTrailerWidth<
    T, std::void_t<decltype(std::declval<T>().trailerWidthMeters)>>
    : std::true_type {};

static_assert(
    !HasCpmTrailerFrontOverhang<CpmTrailerData>::value,
    "CPM R2 TrailerData must not expose frontOverhang");

static_assert(
    !HasCpmTrailerRearOverhang<CpmTrailerData>::value,
    "CPM R2 TrailerData must not expose rearOverhang");

static_assert(
    !HasCpmTrailerWidth<CpmTrailerData>::value,
    "CPM R2 TrailerData must not expose trailerWidth");


template <typename T, typename = void>
struct HasCpmReceivedAtUnixMs : std::false_type {};

template <typename T>
struct HasCpmReceivedAtUnixMs<
    T,
    std::void_t<decltype(std::declval<T>().receivedAtUnixMs)>>
    : std::true_type {};

static_assert(!std::is_same_v<CpmInput, CpmMessage>,
              "Input and decoded message must be distinct types");

static_assert(std::is_base_of_v<CpmSemanticData, CpmInput>,
              "CPM input must expose the shared semantic data");

static_assert(std::is_base_of_v<CpmSemanticData, CpmMessage>,
              "CPM message must expose the shared semantic data");

static_assert(!HasCpmReceivedAtUnixMs<CpmInput>::value,
              "Input must not expose the message reception timestamp");

static_assert(HasCpmReceivedAtUnixMs<CpmMessage>::value,
              "Message must expose its reception timestamp");

static_assert(
    std::is_same_v<decltype(CpmMessage::receivedAtUnixMs), std::int64_t>,
    "Reception timestamp must preserve signed 64-bit milliseconds");


// MapReference deve preservar os identificadores de região opcionais.
static_assert(
    std::is_same_v<
        decltype(CpmRoadSegmentReference::id),
        std::uint16_t>,
    "Road-segment reference ID must preserve Identifier2B width");

static_assert(
    std::is_same_v<
        decltype(CpmRoadSegmentReference::region),
        std::optional<std::uint16_t>>,
    "Road-segment reference must preserve optional region");

static_assert(
    std::is_same_v<
        decltype(CpmIntersectionReference::id),
        std::uint16_t>,
    "Intersection reference ID must preserve Identifier2B width");

static_assert(
    std::is_same_v<
        decltype(CpmIntersectionReference::region),
        std::optional<std::uint16_t>>,
    "Intersection reference must preserve optional region");

// As referências das formas usam coordenadas sem campos de confiança.
static_assert(
    std::is_same_v<
        decltype(CpmRectangularShape::referencePoint),
        std::optional<CpmCartesianPosition3D>>,
    "Rectangular shape reference point must use raw Cartesian coordinates");

static_assert(
    std::is_same_v<
        decltype(CpmCircularShape::referencePoint),
        std::optional<CpmCartesianPosition3D>>,
    "Circular shape reference point must use raw Cartesian coordinates");

static_assert(
    std::is_same_v<
        decltype(CpmEllipticalShape::referencePoint),
        std::optional<CpmCartesianPosition3D>>,
    "Elliptical shape reference point must use raw Cartesian coordinates");

static_assert(
    std::is_same_v<
        decltype(CpmPolygonalShape::referencePoint),
        std::optional<CpmCartesianPosition3D>>,
    "Polygonal shape reference point must use raw Cartesian coordinates");

static_assert(
    std::is_same_v<
        decltype(CpmRadialShape::referencePoint),
        std::optional<CpmCartesianPosition3D>>,
    "Radial shape reference point must use raw Cartesian coordinates");

// Verifica a restrição da alternativa ObjectClass.groupSubClass:
// clusterBoundingBoxShape tem de estar ausente, não apenas limitado
// a um subconjunto de formas geométricas.
template <typename T, typename = void>
struct HasClusterBoundingBoxShapeMember : std::false_type
{
};

template <typename T>
struct HasClusterBoundingBoxShapeMember<
    T,
    std::void_t<decltype(std::declval<T>().clusterBoundingBoxShape)>>
    : std::true_type
{
};

static_assert(
    !HasClusterBoundingBoxShapeMember<CpmGroupObjectClass>::value,
    "CPM R2 ObjectClass.groupSubClass must exclude clusterBoundingBoxShape");

// Os valores têm de corresponder à restrição ASN.1 R2:
// unknown | passengerCar..tram | agricultural.
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::Unknown) == 0,
    "vehicleSubClass unknown must retain ASN.1 value 0");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::PassengerCar) == 5,
    "vehicleSubClass passengerCar must retain ASN.1 value 5");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::Bus) == 6,
    "vehicleSubClass bus must retain ASN.1 value 6");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::LightTruck) == 7,
    "vehicleSubClass lightTruck must retain ASN.1 value 7");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::HeavyTruck) == 8,
    "vehicleSubClass heavyTruck must retain ASN.1 value 8");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::Trailer) == 9,
    "vehicleSubClass trailer must retain ASN.1 value 9");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::SpecialVehicle) == 10,
    "vehicleSubClass specialVehicle must retain ASN.1 value 10");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::Tram) == 11,
    "vehicleSubClass tram must retain ASN.1 value 11");
static_assert(
    static_cast<unsigned>(CpmVehicleSubClassCode::Agricultural) == 14,
    "vehicleSubClass agricultural must retain ASN.1 value 14");


// A precisão física pode estar indisponível; não deve assumir zero.
static_assert(
    std::is_same_v<
        decltype(CpmMeasuredValue::confidence),
        std::optional<double>>,
    "Measured-value accuracy must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(CpmLongitudinalLanePosition::confidence),
        std::optional<double>>,
    "Longitudinal lane-position accuracy must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(CpmAngularVelocityComponent::confidence),
        std::optional<double>>,
    "Angular-velocity accuracy must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(CpmObjectDimension::confidence),
        std::optional<double>>,
    "Object-dimension accuracy must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(CpmConfidenceEllipse::semiMajorConfidenceMeters),
        CpmSemiAxisLength>,
    "Semi-axis length must preserve its domain-specific state");

static_assert(
    std::is_same_v<
        decltype(CpmConfidenceEllipse::semiMinorConfidenceMeters),
        CpmSemiAxisLength>,
    "Semi-minor axis must preserve its domain-specific state");

static_assert(
    std::is_same_v<
        decltype(CpmConfidenceEllipse::semiMajorOrientationDeg),
        CpmHeadingValue>,
    "Ellipse orientation must preserve its domain-specific state");

static_assert(
    std::is_same_v<
        decltype(CpmReferencePosition::altitudeConfidenceMeters),
        CpmAltitudeAccuracy>,
    "Altitude accuracy must preserve known, out-of-range and unavailable");

static_assert(
    std::is_same_v<
        decltype(CpmObjectClassConfidence::confidenceRatio),
        std::optional<CpmConfidenceRatio>>,
    "Object-class confidence ratio must use the dedicated neutral type");

static_assert(
    std::is_same_v<
        decltype(CpmPerceptionRegion::confidenceRatio),
        std::optional<CpmConfidenceRatio>>,
    "Perception-region confidence ratio must use the dedicated neutral type");

static_assert(
    std::is_same_v<
        decltype(CpmSensorInformation::perceptionRegionConfidenceRatio),
        std::optional<CpmConfidenceRatio>>,
    "Sensor confidence ratio must use the dedicated neutral type");

static_assert(
    !std::is_same_v<CpmMeasuredValue, CpmConfidenceRatio>,
    "Physical measurements and confidence ratios must remain distinct types");

static_assert(
    std::is_same_v<
        decltype(CpmReferencePosition::latitudeDeg),
        std::optional<double>>,
    "Reference latitude must preserve the unavailable state");

static_assert(
    std::is_same_v<
        decltype(CpmReferencePosition::longitudeDeg),
        std::optional<double>>,
    "Reference longitude must preserve the unavailable state");

static_assert(
    std::is_same_v<
        decltype(CpmReferencePosition::altitudeMeters),
        std::optional<double>>,
    "Reference altitude must preserve the unavailable state");

static_assert(
    std::is_same_v<
        decltype(CpmVehicleStationData::orientationAngle),
        CpmMeasuredValue>,
    "Vehicle orientation must contain a mandatory value and confidence");

static_assert(
    std::is_same_v<
        decltype(CpmSensorInformation::shadowingApplies),
        bool>,
    "SensorInformation.shadowingApplies is mandatory");

static_assert(
    std::is_same_v<
        decltype(CpmPerceptionRegion::shadowingApplies),
        bool>,
    "PerceptionRegion.shadowingApplies is mandatory");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::velocity),
        std::optional<CpmVelocity3D>>,
    "Velocity must preserve polar/cartesian choice");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::acceleration),
        std::optional<CpmAcceleration3D>>,
    "Acceleration must preserve polar/cartesian choice");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::mapPosition),
        std::optional<CpmMapPosition>>,
    "MapPosition must be represented when present");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::angularVelocity),
        std::optional<CpmAngularVelocityComponent>>,
    "Angular velocity must be represented when present");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::objectAgeMs),
        std::optional<std::int32_t>>,
    "Object age must preserve the signed ASN.1 representation");

static_assert(
    std::is_same_v<
        decltype(CpmVehicleStationData::pitchAngle),
        std::optional<CpmMeasuredValue>>,
    "Vehicle pitch must be optional and carry value/confidence");

static_assert(
    std::is_same_v<
        decltype(CpmVehicleStationData::rollAngle),
        std::optional<CpmMeasuredValue>>,
    "Vehicle roll must be optional and carry value/confidence");

static_assert(
    std::is_same_v<
        decltype(CpmVehicleStationData::trailerDataSet),
        std::optional<std::vector<CpmTrailerData>>>,
    "Vehicle trailer data set must preserve optional presence");

static_assert(
    std::is_same_v<
        decltype(CpmSemanticData::knownObjectCount),
        std::uint8_t>,
    "Global perceived-object count must preserve CardinalNumber1B width");

static_assert(
    std::is_same_v<
        decltype(CpmMessageSegmentationInfo::totalMsgNo),
        std::uint8_t>,
    "Segmentation total must use storage for 1..8; this assertion does not validate the range");

static_assert(
    std::is_same_v<
        decltype(CpmMessageSegmentationInfo::thisMsgNo),
        std::uint8_t>,
    "Segmentation ordinal must use storage for 1..8; this assertion does not validate the range");

static_assert(
    std::is_same_v<
        decltype(CpmSemanticData::segmentationInfo),
        std::optional<CpmMessageSegmentationInfo>>,
    "Segmentation metadata must preserve optional presence");

static_assert(
    std::is_same_v<
        decltype(CpmMessageRateHz::mantissa),
        std::uint8_t>,
    "Message-rate mantissa must represent the ASN.1 range 1..100");

static_assert(
    std::is_same_v<
        decltype(CpmMessageRateHz::exponent),
        std::int8_t>,
    "Message-rate exponent must preserve negative values");

static_assert(
    std::is_same_v<
        decltype(CpmMessageRateRange::messageRateMin),
        CpmMessageRateHz>,
    "Minimum message rate must preserve mantissa and exponent");

static_assert(
    std::is_same_v<
        decltype(CpmMessageRateRange::messageRateMax),
        CpmMessageRateHz>,
    "Maximum message rate must preserve mantissa and exponent");

static_assert(
    std::is_same_v<
        decltype(CpmSemanticData::messageRateRange),
        std::optional<CpmMessageRateRange>>,
    "Message-rate range must preserve optional presence");

static_assert(
    std::is_same_v<
        decltype(CpmPerceptionRegion::numberOfPerceivedObjects),
        std::optional<std::uint8_t>>,
    "Perception-region object count must preserve CardinalNumber1B width");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::sensorIds),
        std::optional<std::vector<std::uint8_t>>>,
    "PerceivedObject.sensorIdList must preserve optional presence");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::classifications),
        std::optional<std::vector<CpmObjectClassConfidence>>>,
    "PerceivedObject.classification must preserve optional presence");

static_assert(
    std::variant_size_v<CpmObjectClass> == 4,
    "ObjectClass must preserve its four ASN.1 alternatives");

static_assert(
    std::variant_size_v<CpmVruSubClass> == 4,
    "VRU classification must preserve its four profile alternatives");

static_assert(
    std::is_same_v<
        decltype(CpmPerceivedObject::correlationMatrices),
        std::optional<std::vector<CpmCorrelationMatrix>>>,
    "Correlation matrices must preserve optional presence");

static_assert(
    std::is_class_v<CpmCorrelationCellValue>,
    "Correlation cell must preserve arbitrary numeric coefficients");


static int TestCpmManagementMetadataValidation()
{
    using Error = CpmManagementMetadataValidationError;
    CpmSemanticData data{};

    // Ambos os blocos de metadados podem estar ausentes.
    if (ValidateCpmManagementMetadata(data) != Error::None) {
        return 1;
    }

    data.segmentationInfo = CpmMessageSegmentationInfo{1, 1};
    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, -5},
        CpmMessageRateHz{100, 2}
    };
    if (ValidateCpmManagementMetadata(data) != Error::None) {
        return 2;
    }

    data.segmentationInfo = CpmMessageSegmentationInfo{8, 8};
    if (ValidateCpmManagementMetadata(data) != Error::None) {
        return 3;
    }

    data.segmentationInfo = CpmMessageSegmentationInfo{0, 1};
    if (ValidateCpmManagementMetadata(data) !=
        Error::SegmentationTotalOutOfRange) {
        return 4;
    }

    data.segmentationInfo = CpmMessageSegmentationInfo{9, 1};
    if (ValidateCpmManagementMetadata(data) !=
        Error::SegmentationTotalOutOfRange) {
        return 5;
    }

    data.segmentationInfo = CpmMessageSegmentationInfo{8, 0};
    if (ValidateCpmManagementMetadata(data) !=
        Error::SegmentationOrdinalOutOfRange) {
        return 6;
    }

    data.segmentationInfo = CpmMessageSegmentationInfo{8, 9};
    if (ValidateCpmManagementMetadata(data) !=
        Error::SegmentationOrdinalOutOfRange) {
        return 7;
    }

    data.segmentationInfo = CpmMessageSegmentationInfo{2, 3};
    if (ValidateCpmManagementMetadata(data) !=
        Error::SegmentationOrdinalExceedsTotal) {
        return 8;
    }

    data.segmentationInfo.reset();

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{0, 0}, CpmMessageRateHz{100, 0}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMinMantissaOutOfRange) {
        return 9;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{101, 0}, CpmMessageRateHz{100, 2}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMinMantissaOutOfRange) {
        return 10;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, -6}, CpmMessageRateHz{100, 2}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMinExponentOutOfRange) {
        return 11;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, 3}, CpmMessageRateHz{100, 2}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMinExponentOutOfRange) {
        return 12;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, -5}, CpmMessageRateHz{0, 0}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMaxMantissaOutOfRange) {
        return 13;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, -5}, CpmMessageRateHz{101, 0}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMaxMantissaOutOfRange) {
        return 14;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, -5}, CpmMessageRateHz{1, -6}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMaxExponentOutOfRange) {
        return 15;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, -5}, CpmMessageRateHz{1, 3}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMaxExponentOutOfRange) {
        return 16;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{2, -4}, CpmMessageRateHz{1, -3}
    };
    if (ValidateCpmManagementMetadata(data) != Error::None) {
        return 17;
    }

    // 1 * 10^2 e 100 * 10^0 representam a mesma taxa.
    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, 2}, CpmMessageRateHz{100, 0}
    };
    if (ValidateCpmManagementMetadata(data) != Error::None) {
        return 18;
    }

    data.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{2, -4}, CpmMessageRateHz{1, -5}
    };
    if (ValidateCpmManagementMetadata(data) !=
        Error::MessageRateMinExceedsMax) {
        return 19;
    }

    return 0;
}


static int TestCpmConfidenceRatioValidation()
{
    using Error = CpmConfidenceRatioValidationError;

    const auto validateClassRatio = [](double value) {
        CpmSemanticData data{};
        CpmPerceivedObject object{};
        CpmObjectClassConfidence classification{};
        classification.confidenceRatio = CpmConfidenceRatio{value};
        object.classifications =
            std::vector<CpmObjectClassConfidence>{classification};
        data.perceivedObjects.push_back(object);
        return ValidateCpmConfidenceRatios(data);
    };

    const auto validateSensorRatio = [](double value) {
        CpmSemanticData data{};
        CpmSensorInformation sensor{};
        sensor.perceptionRegionConfidenceRatio = CpmConfidenceRatio{value};
        data.sensorInformation.push_back(sensor);
        return ValidateCpmConfidenceRatios(data);
    };

    const auto validateRegionRatio = [](double value) {
        CpmSemanticData data{};
        CpmPerceptionRegion region{};
        region.confidenceRatio = CpmConfidenceRatio{value};
        data.perceptionRegions.push_back(region);
        return ValidateCpmConfidenceRatios(data);
    };

    // Opcionalidade: dados sem razões e razões indisponíveis são válidos.
    CpmSemanticData empty{};
    if (ValidateCpmConfidenceRatios(empty) != Error::None) {
        return 1;
    }

    // Os extremos do domínio neutro são preservados.
    if (validateClassRatio(0.0) != Error::None ||
        validateClassRatio(1.0) != Error::None ||
        validateSensorRatio(0.0) != Error::None ||
        validateSensorRatio(1.0) != Error::None ||
        validateRegionRatio(0.0) != Error::None ||
        validateRegionRatio(1.0) != Error::None) {
        return 2;
    }

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    if (validateClassRatio(nan) != Error::ObjectClassRatioNonFinite ||
        validateClassRatio(infinity) != Error::ObjectClassRatioNonFinite) {
        return 3;
    }

    if (validateSensorRatio(nan) != Error::SensorRatioNonFinite ||
        validateSensorRatio(-infinity) != Error::SensorRatioNonFinite) {
        return 4;
    }

    if (validateRegionRatio(nan) != Error::PerceptionRegionRatioNonFinite ||
        validateRegionRatio(infinity) !=
            Error::PerceptionRegionRatioNonFinite) {
        return 5;
    }

    if (validateClassRatio(-0.01) != Error::ObjectClassRatioOutOfRange ||
        validateClassRatio(1.01) != Error::ObjectClassRatioOutOfRange) {
        return 6;
    }

    if (validateSensorRatio(-0.01) != Error::SensorRatioOutOfRange ||
        validateSensorRatio(1.01) != Error::SensorRatioOutOfRange) {
        return 7;
    }

    if (validateRegionRatio(-0.01) !=
            Error::PerceptionRegionRatioOutOfRange ||
        validateRegionRatio(1.01) !=
            Error::PerceptionRegionRatioOutOfRange) {
        return 8;
    }

    return 0;
}

int main()
{
    const int metadataValidationCase =
        TestCpmManagementMetadataValidation();
    if (metadataValidationCase != 0) {
        return 100 + metadataValidationCase;
    }

    const int confidenceRatioValidationCase =
        TestCpmConfidenceRatioValidation();
    if (confidenceRatioValidationCase != 0) {
        return 120 + confidenceRatioValidationCase;
    }

    const CpmMeasuredValue unknownMeasurement{};
    const CpmMeasuredValue knownMeasurement{12.0, 0.25};

    if (unknownMeasurement.confidence.has_value() ||
        !knownMeasurement.confidence.has_value() ||
        *knownMeasurement.confidence != 0.25) {
        return 21;
    }


    CpmInput input{};

    input.station.stationId = 9001;
    CpmVehicleStationData vehicle{};
    vehicle.orientationAngle = CpmMeasuredValue{0.0, 1.0};
    vehicle.pitchAngle = CpmMeasuredValue{1.0, 0.5};
    vehicle.rollAngle = CpmMeasuredValue{-1.0, 0.5};

    CpmTrailerData trailer{};
    trailer.referencePointId = 1;
    trailer.hitchPointOffsetMeters = 1.2;
    trailer.hitchAngle = CpmMeasuredValue{5.0, 1.0};
    vehicle.trailerDataSet = std::vector<CpmTrailerData>{trailer};

    input.station.origin = vehicle;

    const auto* vehicleOrigin =
        std::get_if<CpmVehicleStationData>(&input.station.origin);
    if (vehicleOrigin == nullptr ||
        vehicleOrigin->orientationAngle.value != 0.0 ||
        vehicleOrigin->orientationAngle.confidence != 1.0 ||
        !vehicleOrigin->pitchAngle.has_value() ||
        !vehicleOrigin->rollAngle.has_value() ||
        !vehicleOrigin->trailerDataSet.has_value() ||
        vehicleOrigin->trailerDataSet->size() != 1 ||
        vehicleOrigin->trailerDataSet->front().referencePointId != 1) {
        return 7;
    }

    // MapReference é opcional e escolhe exatamente uma alternativa.
    CpmRoadSideUnitStationData rsuOrigin{};
    rsuOrigin.mapReference = CpmRoadSegmentReference{77, std::nullopt};

    CpmStationData rsuStation{};
    rsuStation.stationId = 9100;
    rsuStation.origin = rsuOrigin;

    const auto* selectedRsu =
        std::get_if<CpmRoadSideUnitStationData>(&rsuStation.origin);
    if (selectedRsu == nullptr || !selectedRsu->mapReference.has_value()) {
        return 4;
    }

    const auto* roadSegment =
        std::get_if<CpmRoadSegmentReference>(&*selectedRsu->mapReference);
    if (roadSegment == nullptr || roadSegment->id != 77) {
        return 5;
    }

    CpmMapReference intersectionReference = CpmIntersectionReference{43, std::nullopt};
    const auto* intersection =
        std::get_if<CpmIntersectionReference>(&intersectionReference);
    if (intersection == nullptr || intersection->id != 43) {
        return 6;
    }

    // A origem da estação escolhe Vehicle ou RSU; a referência de mapa
    // do RSU escolhe, por sua vez, roadsegment ou intersection.

    input.referenceTimeItsMs = 123456;
    input.referencePosition.latitudeDeg = 0.0;
    input.referencePosition.longitudeDeg = 0.0;
    input.referencePosition.altitudeMeters = 0.0;
    input.referencePosition.confidenceEllipse.semiMajorConfidenceMeters = CpmSemiAxisLength{CpmSemiAxisState::Known, 1.0};
    input.referencePosition.confidenceEllipse.semiMinorConfidenceMeters = CpmSemiAxisLength{CpmSemiAxisState::Known, 1.0};
    input.referencePosition.confidenceEllipse.semiMajorOrientationDeg = CpmHeadingValue{CpmHeadingState::Known, 0.0};

    // A contagem global não é a contagem local de uma região.
    CpmSemanticData emptyMetadata{};
    if (emptyMetadata.segmentationInfo.has_value() ||
        emptyMetadata.messageRateRange.has_value()) {
        return 10;
    }

    // Testar os extremos de representação sem aplicar conversão ASN.1.
    CpmInput boundaryInput{};
    boundaryInput.knownObjectCount = 255;
    boundaryInput.segmentationInfo = CpmMessageSegmentationInfo{8, 8};
    boundaryInput.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, -5},
        CpmMessageRateHz{100, 2}
    };

    CpmMessage boundaryMessage{};
    static_cast<CpmSemanticData&>(boundaryMessage) =
        static_cast<const CpmSemanticData&>(boundaryInput);

    if (boundaryMessage.knownObjectCount != 255 ||
        !boundaryMessage.segmentationInfo.has_value() ||
        boundaryMessage.segmentationInfo->totalMsgNo != 8 ||
        boundaryMessage.segmentationInfo->thisMsgNo != 8 ||
        !boundaryMessage.messageRateRange.has_value() ||
        boundaryMessage.messageRateRange->messageRateMin.mantissa != 1 ||
        boundaryMessage.messageRateRange->messageRateMin.exponent != -5 ||
        boundaryMessage.messageRateRange->messageRateMax.mantissa != 100 ||
        boundaryMessage.messageRateRange->messageRateMax.exponent != 2) {
        return 20;
    }

    input.knownObjectCount = 2;

    input.segmentationInfo = CpmMessageSegmentationInfo{3, 2};
    input.messageRateRange = CpmMessageRateRange{
        CpmMessageRateHz{1, 0},
        CpmMessageRateHz{10, 0}
    };

    CpmPerceivedObject object{};
    object.objectId = 501;
    object.measurementDeltaTimeMs = -12;
    object.position.xEast = {1200.0, 1.0};
    object.position.yNorth = {-350.0, 2.0};
    object.position.zUp = CpmCoordinateWithConfidence{0.0, 1.0};

    CpmVelocityCartesian cartesianVelocity{};
    cartesianVelocity.x = {10.0, 0.1};
    cartesianVelocity.y = {2.0, 0.1};
    cartesianVelocity.z = CpmVelocityComponentWithConfidence{0.0, 0.1};
    object.velocity = CpmVelocity3D{cartesianVelocity};

    CpmVelocityPolar polarVelocity{};
    polarVelocity.magnitude = {14.0, 0.2};
    polarVelocity.direction = {45.0, 1.0};
    polarVelocity.z = CpmVelocityComponentWithConfidence{0.0, 0.1};

    CpmVelocity3D polarVelocityChoice{polarVelocity};
    if (!std::holds_alternative<CpmVelocityPolar>(polarVelocityChoice)) {
        return 8;
    }

    CpmAccelerationCartesian cartesianAcceleration{};
    cartesianAcceleration.x = {0.5, 0.1};
    cartesianAcceleration.y = {0.0, 0.1};
    object.acceleration = CpmAcceleration3D{cartesianAcceleration};

    CpmAccelerationPolar polarAcceleration{};
    polarAcceleration.magnitude = {0.5, 0.1};
    polarAcceleration.direction = {0.0, 1.0};
    CpmAcceleration3D polarAccelerationChoice{polarAcceleration};

    if (!std::holds_alternative<CpmAccelerationPolar>(
            polarAccelerationChoice)) {
        return 9;
    }

    object.angularVelocity = CpmAngularVelocityComponent{2.0, 0.5};

    CpmMapPosition mapPosition{};
    mapPosition.mapReference = CpmIntersectionReference{43, std::nullopt};
    mapPosition.laneId = 2;
    mapPosition.connectionId = 7;
    mapPosition.longitudinalLanePosition =
        CpmLongitudinalLanePosition{10.0, 0.2};
    object.mapPosition = mapPosition;

    CpmCorrelationMatrix correlationMatrix{};
    correlationMatrix.includedComponents.set(
        static_cast<std::size_t>(CpmCorrelationComponent::XPosition));
    correlationMatrix.includedComponents.set(
        static_cast<std::size_t>(CpmCorrelationComponent::YPosition));
    correlationMatrix.includedComponents.set(
        static_cast<std::size_t>(CpmCorrelationComponent::ZAngularVelocity));

    CpmCorrelationColumn correlationColumn{};
    correlationColumn.values = {
        CpmCorrelationCellValue::FullNegativeCorrelation,
        CpmCorrelationCellValue::NoCorrelation,
        CpmCorrelationCellValue::FullPositiveCorrelation,
        CpmCorrelationCellValue::Unavailable
    };
    correlationColumn.values.push_back(CpmCorrelationCellValue{-37});
    if (correlationColumn.values.back().value != -37) {
        return 17;
    }
    correlationMatrix.columns.push_back(correlationColumn);
    object.correlationMatrices =
        std::vector<CpmCorrelationMatrix>{correlationMatrix};

    if (!object.correlationMatrices.has_value() ||
        object.correlationMatrices->size() != 1 ||
        object.correlationMatrices->front().columns.size() != 1 ||
        object.correlationMatrices->front().columns.front().values.size() != 5 ||
        !object.correlationMatrices->front().includedComponents.test(
            static_cast<std::size_t>(CpmCorrelationComponent::ZAngularVelocity))) {
        return 14;
    }

    CpmPerceivedObject absentCorrelation{};
    if (absentCorrelation.correlationMatrices.has_value()) {
        return 15;
    }
    absentCorrelation.correlationMatrices =
        std::vector<CpmCorrelationMatrix>{};
    if (!absentCorrelation.correlationMatrices.has_value() ||
        !absentCorrelation.correlationMatrices->empty()) {
        return 16;
    }

    object.angles = CpmObjectAngles{};
    object.angles->zAngleDeg = {90.0, 1.0};

    object.dimensionX = CpmObjectDimension{4.2, 0.1};
    object.objectAgeMs = -1;
    object.objectPerceptionQuality = 12;
    object.sensorIds = std::vector<std::uint8_t>{1};

    // O modelo neutro preserva valores presentes nos dois extremos
    // e distingue ambos de uma confiança indisponível. Isto testa apenas
    // o contrato do modelo; não afirma que todos os valores são codificáveis.
    CpmObjectClassConfidence ratioZeroClass{};
    CpmObjectClassConfidence ratioFullClass{};
    CpmObjectClassConfidence ratioUnavailableClass{};
    ratioZeroClass.confidenceRatio = CpmConfidenceRatio{0.0};
    ratioFullClass.confidenceRatio = CpmConfidenceRatio{1.0};

    CpmSensorInformation ratioZeroSensor{};
    CpmSensorInformation ratioFullSensor{};
    CpmSensorInformation ratioUnavailableSensor{};
    ratioZeroSensor.perceptionRegionConfidenceRatio = CpmConfidenceRatio{0.0};
    ratioFullSensor.perceptionRegionConfidenceRatio = CpmConfidenceRatio{1.0};

    CpmPerceptionRegion ratioZeroRegion{};
    CpmPerceptionRegion ratioFullRegion{};
    CpmPerceptionRegion ratioUnavailableRegion{};
    ratioZeroRegion.confidenceRatio = CpmConfidenceRatio{0.0};
    ratioFullRegion.confidenceRatio = CpmConfidenceRatio{1.0};

    if (!ratioZeroClass.confidenceRatio.has_value() ||
        ratioZeroClass.confidenceRatio->value != 0.0 ||
        !ratioFullClass.confidenceRatio.has_value() ||
        ratioFullClass.confidenceRatio->value != 1.0 ||
        ratioUnavailableClass.confidenceRatio.has_value() ||
        !ratioZeroSensor.perceptionRegionConfidenceRatio.has_value() ||
        ratioZeroSensor.perceptionRegionConfidenceRatio->value != 0.0 ||
        !ratioFullSensor.perceptionRegionConfidenceRatio.has_value() ||
        ratioFullSensor.perceptionRegionConfidenceRatio->value != 1.0 ||
        ratioUnavailableSensor.perceptionRegionConfidenceRatio.has_value() ||
        !ratioZeroRegion.confidenceRatio.has_value() ||
        ratioZeroRegion.confidenceRatio->value != 0.0 ||
        !ratioFullRegion.confidenceRatio.has_value() ||
        ratioFullRegion.confidenceRatio->value != 1.0 ||
        ratioUnavailableRegion.confidenceRatio.has_value()) {
        return 90;
    }

    CpmObjectClassConfidence classification{};
    classification.objectClass =
        CpmObjectClass{CpmVehicleObjectClass{
            CpmVehicleSubClassCode::PassengerCar}};
    classification.confidenceRatio = CpmConfidenceRatio{0.9};
    object.classifications =
        std::vector<CpmObjectClassConfidence>{classification};

    CpmObjectClass vruClass = CpmVruObjectClass{
        CpmVruSubClass{CpmVruPedestrianSubClassCode::OrdinaryPedestrian}};
    CpmObjectClass groupClass = CpmGroupObjectClass{};
    CpmObjectClass otherClass =
        CpmOtherObjectClass{CpmOtherSubClassCode::SingleObject};

    if (!std::holds_alternative<CpmVruObjectClass>(vruClass) ||
        !std::holds_alternative<CpmGroupObjectClass>(groupClass) ||
        !std::holds_alternative<CpmOtherObjectClass>(otherClass)) {
        return 11;
    }

    CpmPerceivedObject optionalLists{};
    if (optionalLists.sensorIds.has_value() ||
        optionalLists.classifications.has_value()) {
        return 12;
    }

    optionalLists.sensorIds = std::vector<std::uint8_t>{};
    optionalLists.classifications =
        std::vector<CpmObjectClassConfidence>{};

    if (!optionalLists.sensorIds.has_value() ||
        !optionalLists.sensorIds->empty() ||
        !optionalLists.classifications.has_value() ||
        !optionalLists.classifications->empty()) {
        return 13;
    }

    input.perceivedObjects.push_back(object);

    CpmSensorInformation sensor{};
    sensor.sensorId = 1;
    sensor.sensorType = CpmSensorKind::Radar;
    sensor.perceptionRegionShape =
        CpmRegionShape{CpmCircularShape{std::nullopt, 30.0, std::nullopt}};
    sensor.perceptionRegionConfidenceRatio = CpmConfidenceRatio{0.95};
    sensor.shadowingApplies = false;
    input.sensorInformation.push_back(sensor);

    CpmPolygonalShape polygon{};
    polygon.vertices.push_back({0.0, 0.0, std::nullopt});
    polygon.vertices.push_back({10.0, 0.0, std::nullopt});
    polygon.vertices.push_back({0.0, 10.0, std::nullopt});

    CpmPerceptionRegion region{};
    region.measurementDeltaTimeMs = 0;
    region.confidenceRatio = CpmConfidenceRatio{0.95};
    region.shape = polygon;
    region.shadowingApplies = false;
    region.sensorIds = std::vector<uint8_t>{1};
    region.numberOfPerceivedObjects = 2;
    region.perceivedObjectIds = std::vector<uint16_t>{501};
    input.perceptionRegions.push_back(region);

    // O domínio neutro representa todas as variantes de geometria R2.
    CpmRegionShape rectangular = CpmRectangularShape{};
    CpmRegionShape circular = CpmCircularShape{};
    CpmRegionShape elliptical = CpmEllipticalShape{};
    CpmRegionShape polygonal = CpmPolygonalShape{};
    CpmRegionShape radial = CpmRadialShape{};
    CpmRegionShape radialShapes = CpmRadialShapes{};

    if (!std::holds_alternative<CpmRectangularShape>(rectangular) ||
        !std::holds_alternative<CpmCircularShape>(circular) ||
        !std::holds_alternative<CpmEllipticalShape>(elliptical) ||
        !std::holds_alternative<CpmPolygonalShape>(polygonal) ||
        !std::holds_alternative<CpmRadialShape>(radial) ||
        !std::holds_alternative<CpmRadialShapes>(radialShapes)) {
        return 2;
    }

    CpmMessage message{};
    // Estado semântico: eixo maior indisponível, eixo menor conhecido,
    // orientação conhecida igual a zero graus e precisão de altitude conhecida.
    input.referencePosition.confidenceEllipse.semiMajorConfidenceMeters =
        CpmSemiAxisLength{CpmSemiAxisState::Unavailable, 0.0};
    input.referencePosition.confidenceEllipse.semiMinorConfidenceMeters =
        CpmSemiAxisLength{CpmSemiAxisState::Known, 3.2};
    input.referencePosition.confidenceEllipse.semiMajorOrientationDeg =
        CpmHeadingValue{CpmHeadingState::Known, 0.0};
    input.referencePosition.altitudeConfidenceMeters =
        CpmAltitudeAccuracy{CpmAltitudeAccuracyState::Known, 1.5};

    static_cast<CpmSemanticData&>(message) =
        static_cast<const CpmSemanticData&>(input);
    // Zero conhecido e coordenada indisponível são estados distintos.
    CpmInput georefStateInput{};
    georefStateInput.referencePosition.latitudeDeg = 0.0;
    georefStateInput.referencePosition.longitudeDeg = std::nullopt;
    georefStateInput.referencePosition.altitudeMeters = 0.0;

    CpmMessage georefStateMessage{};
    static_cast<CpmSemanticData&>(georefStateMessage) =
        static_cast<const CpmSemanticData&>(georefStateInput);

    if (!georefStateMessage.referencePosition.latitudeDeg.has_value() ||
        *georefStateMessage.referencePosition.latitudeDeg != 0.0 ||
        georefStateMessage.referencePosition.longitudeDeg.has_value() ||
        !georefStateMessage.referencePosition.altitudeMeters.has_value() ||
        *georefStateMessage.referencePosition.altitudeMeters != 0.0) {
        return 23;
    }

    message.receivedAtUnixMs = 123456789;
    if (message.referencePosition.confidenceEllipse
            .semiMajorConfidenceMeters.state !=
                CpmSemiAxisState::Unavailable ||
        message.referencePosition.confidenceEllipse
            .semiMinorConfidenceMeters.state != CpmSemiAxisState::Known ||
        message.referencePosition.confidenceEllipse
            .semiMinorConfidenceMeters.meters != 3.2 ||
        message.referencePosition.confidenceEllipse
            .semiMajorOrientationDeg.state != CpmHeadingState::Known ||
        message.referencePosition.confidenceEllipse
            .semiMajorOrientationDeg.degrees != 0.0 ||
        message.referencePosition.altitudeConfidenceMeters.state !=
            CpmAltitudeAccuracyState::Known ||
        message.referencePosition.altitudeConfidenceMeters.meters != 1.5) {
        return 22;
    }


    if (message.station.stationId != 9001 ||
        message.knownObjectCount != 2 ||
        message.segmentationInfo.has_value() == false ||
        message.segmentationInfo->totalMsgNo != 3 ||
        message.segmentationInfo->thisMsgNo != 2 ||
        message.messageRateRange.has_value() == false ||
        message.messageRateRange->messageRateMin.mantissa != 1 ||
        message.messageRateRange->messageRateMin.exponent != 0 ||
        message.messageRateRange->messageRateMax.mantissa != 10 ||
        message.messageRateRange->messageRateMax.exponent != 0 ||
        message.perceivedObjects.size() != 1 ||
        message.perceivedObjects.front().objectId != 501 ||
        message.receivedAtUnixMs != 123456789) {
        return 3;
    }

    return 0;
}
