#ifndef VANETZA_INTEGRATION_CPM_TYPES_CPM_DOMAIN_TYPES_H
#define VANETZA_INTEGRATION_CPM_TYPES_CPM_DOMAIN_TYPES_H

#include <cstdint>
#include <cstddef>
#include <bitset>
#include <optional>
#include <variant>
#include <vector>

enum class CpmSensorKind
{
    Undefined,
    Radar,
    Lidar,
    Monovideo,
    StereoVision,
    NightVision,
    Ultrasonic,
    Pmd,
    InductionLoop,
    SphericalCamera,
    Uwb,
    Acoustic,
    LocalAggregation,
    ItsAggregation,
    Rfid
};

// Valor físico neutro e estimativa de precisão absoluta nas unidades
// do campo. confidence não é uma razão probabilística; nullopt significa
// que a precisão não está disponível. O mapper deve aplicar a escala,
// quantização e códigos reservados específicos do tipo ASN.1 de destino.
struct CpmMeasuredValue
{
    double value{};
    std::optional<double> confidence;
};

using CpmCoordinateWithConfidence = CpmMeasuredValue;

struct CpmPosition3D
{
    CpmCoordinateWithConfidence xEast{};
    CpmCoordinateWithConfidence yNorth{};
    std::optional<CpmCoordinateWithConfidence> zUp;
};

struct CpmCartesianPosition3D
{
    double xEastMeters{};
    double yNorthMeters{};
    std::optional<double> zUpMeters;
};

// Parâmetros da elipse de confiança no modelo neutro.
// nullopt significa estimativa indisponível, não ausência de campo ASN.1:
// os componentes de PosConfidenceEllipse são obrigatórios no fio e o mapper
// deverá codificar o valor reservado de indisponibilidade do domínio.
// Estados neutros da precisão da elipse. meters contém uma estimativa
// física, não o inteiro escalado transmitido no ASN.1.
enum class CpmSemiAxisState : std::uint8_t
{
    Known,
    DoNotUse,
    OutOfRange,
    Unavailable
};

struct CpmSemiAxisLength
{
    CpmSemiAxisState state{CpmSemiAxisState::Unavailable};
    double meters{};
};

// Orientação WGS84: graus no domínio neutro; a escala ASN.1 é aplicada
// apenas no mapper.
enum class CpmHeadingState : std::uint8_t
{
    Known,
    DoNotUse,
    Unavailable
};

struct CpmHeadingValue
{
    CpmHeadingState state{CpmHeadingState::Unavailable};
    double degrees{};
};

// A precisão da altitude é uma estimativa física. O mapper converte-a
// para a classe discreta AltitudeConfidence definida pelo CDD.
enum class CpmAltitudeAccuracyState : std::uint8_t
{
    Known,
    OutOfRange,
    Unavailable
};

struct CpmAltitudeAccuracy
{
    CpmAltitudeAccuracyState state{CpmAltitudeAccuracyState::Unavailable};
    double meters{};
};

// Os componentes são obrigatórios no ASN.1.
// O estado neutro identifica o significado antes da codificação.
struct CpmConfidenceEllipse
{
    CpmSemiAxisLength semiMajorConfidenceMeters{};
    CpmSemiAxisLength semiMinorConfidenceMeters{};
    CpmHeadingValue semiMajorOrientationDeg{};
};

// Os três campos são obrigatórios no ASN.1 ReferencePosition.
// nullopt representa indisponibilidade sem omitir o campo no fio;
// o mapper deve codificar o valor reservado próprio de cada domínio.
// Os campos são obrigatórios no ASN.1. nullopt representa um valor
// geográfico indisponível; o mapper deve escrever o código reservado.
struct CpmReferencePosition
{
    std::optional<double> latitudeDeg;
    std::optional<double> longitudeDeg;
    std::optional<double> altitudeMeters;
    CpmConfidenceEllipse confidenceEllipse{};
    CpmAltitudeAccuracy altitudeConfidenceMeters{};
};

struct CpmTrailerData
{
    std::uint8_t referencePointId{};
    double hitchPointOffsetMeters{};
    CpmMeasuredValue hitchAngle{};
};

struct CpmVehicleStationData
{
    CpmMeasuredValue orientationAngle{};
    std::optional<CpmMeasuredValue> pitchAngle;
    std::optional<CpmMeasuredValue> rollAngle;
    std::optional<std::vector<CpmTrailerData>> trailerDataSet;
};

struct CpmRoadSegmentReference
{
    std::uint16_t id{};
    std::optional<std::uint16_t> region;
};

struct CpmIntersectionReference
{
    std::uint16_t id{};
    std::optional<std::uint16_t> region;
};

// Corresponde à escolha exclusiva de MapReference no ASN.1 R2.
using CpmMapReference = std::variant<
    CpmRoadSegmentReference,
    CpmIntersectionReference>;

struct CpmRoadSideUnitStationData
{
    std::optional<CpmMapReference> mapReference;
};

// O container de origem seleciona exatamente uma alternativa.
using CpmStationOrigin = std::variant<
    CpmVehicleStationData,
    CpmRoadSideUnitStationData>;

struct CpmStationData
{
    std::uint32_t stationId{};
    CpmStationOrigin origin{CpmVehicleStationData{}};
};

struct CpmLongitudinalLanePosition
{
    double value{};
    std::optional<double> confidence;
};

struct CpmMapPosition
{
    std::optional<CpmMapReference> mapReference;
    std::optional<std::uint8_t> laneId;
    std::optional<std::uint8_t> connectionId;
    std::optional<CpmLongitudinalLanePosition> longitudinalLanePosition;
};

// Tipos de movimento no domínio neutro. A conversão para os códigos
// de confiança discretos do ASN.1 será responsabilidade do mapper.
using CpmVelocityComponentWithConfidence = CpmMeasuredValue;
using CpmAccelerationComponentWithConfidence = CpmMeasuredValue;

struct CpmVelocityCartesian
{
    CpmVelocityComponentWithConfidence x{};
    CpmVelocityComponentWithConfidence y{};
    std::optional<CpmVelocityComponentWithConfidence> z;
};

struct CpmVelocityPolar
{
    CpmMeasuredValue magnitude{};
    CpmMeasuredValue direction{};
    std::optional<CpmVelocityComponentWithConfidence> z;
};

using CpmVelocity3D = std::variant<CpmVelocityPolar, CpmVelocityCartesian>;

struct CpmAccelerationCartesian
{
    CpmAccelerationComponentWithConfidence x{};
    CpmAccelerationComponentWithConfidence y{};
    std::optional<CpmAccelerationComponentWithConfidence> z;
};

struct CpmAccelerationPolar
{
    CpmMeasuredValue magnitude{};
    CpmMeasuredValue direction{};
    std::optional<CpmAccelerationComponentWithConfidence> z;
};

using CpmAcceleration3D =
    std::variant<CpmAccelerationPolar, CpmAccelerationCartesian>;

struct CpmAngularVelocityComponent
{
    double value{};
    std::optional<double> confidence;
};

struct CpmObjectAngles
{
    CpmMeasuredValue zAngleDeg{};
    std::optional<CpmMeasuredValue> yAngleDeg;
    std::optional<CpmMeasuredValue> xAngleDeg;
};

struct CpmObjectDimension
{
    double meters{};
    std::optional<double> confidence;
};

struct CpmRectangularShape
{
    std::optional<CpmCartesianPosition3D> referencePoint;
    double semiLengthMeters{};
    double semiBreadthMeters{};
    std::optional<double> orientationDeg;
    std::optional<double> heightMeters;
};

struct CpmCircularShape
{
    std::optional<CpmCartesianPosition3D> referencePoint;
    double radiusMeters{};
    std::optional<double> heightMeters;
};

struct CpmEllipticalShape
{
    std::optional<CpmCartesianPosition3D> referencePoint;
    double semiMajorAxisLengthMeters{};
    double semiMinorAxisLengthMeters{};
    std::optional<double> orientationDeg;
    std::optional<double> heightMeters;
};

struct CpmPolygonVertex
{
    double xEastMeters{};
    double yNorthMeters{};
    std::optional<double> zUpMeters;
};

struct CpmPolygonalShape
{
    std::optional<CpmCartesianPosition3D> referencePoint;
    std::vector<CpmPolygonVertex> vertices;
    std::optional<double> heightMeters;
};

struct CpmRadialShape
{
    std::optional<CpmCartesianPosition3D> referencePoint;
    double rangeMeters{};
    double horizontalOpeningAngleStartDeg{};
    double horizontalOpeningAngleEndDeg{};
    std::optional<double> verticalOpeningAngleStartDeg;
    std::optional<double> verticalOpeningAngleEndDeg;
};

struct CpmRadialShapeDetail
{
    double rangeMeters{};
    double horizontalOpeningAngleStartDeg{};
    double horizontalOpeningAngleEndDeg{};
    std::optional<double> verticalOpeningAngleStartDeg;
    std::optional<double> verticalOpeningAngleEndDeg;
};

struct CpmRadialShapes
{
    std::uint8_t referencePointId{};
    double xEastMeters{};
    double yNorthMeters{};
    std::optional<double> zUpMeters;
    std::vector<CpmRadialShapeDetail> shapes;
};

using CpmRegionShape = std::variant<
    CpmRectangularShape,
    CpmCircularShape,
    CpmEllipticalShape,
    CpmPolygonalShape,
    CpmRadialShape,
    CpmRadialShapes>;

// Valores semânticos neutros para os códigos de classe do perfil CPM R2.
enum class CpmVehicleSubClassCode : std::uint8_t
{
    // ObjectClass.vehicleSubClass:
    // unknown | passengerCar..tram | agricultural
    Unknown = 0,
    PassengerCar = 5,
    Bus = 6,
    LightTruck = 7,
    HeavyTruck = 8,
    Trailer = 9,
    SpecialVehicle = 10,
    Tram = 11,
    Agricultural = 14
};

enum class CpmVruPedestrianSubClassCode : std::uint8_t
{
    Unavailable = 0,
    OrdinaryPedestrian = 1,
    RoadWorker = 2,
    FirstResponder = 3
};

enum class CpmVruBicyclistSubClassCode : std::uint8_t
{
    Unavailable = 0,
    Bicyclist = 1,
    WheelchairUser = 2,
    HorseAndRider = 3,
    Rollerskater = 4,
    EScooter = 5,
    PersonalTransporter = 6,
    Pedelec = 7,
    SpeedPedelec = 8,
    Roadbike = 9,
    ChildrensBike = 10,
    Racebike = 11
};

enum class CpmVruMotorcyclistSubClassCode : std::uint8_t
{
    Unavailable = 0,
    Moped = 1,
    Motorcycle = 2,
    MotorcycleAndSidecarRight = 3,
    MotorcycleAndSidecarLeft = 4
};

enum class CpmVruAnimalSubClassCode : std::uint8_t
{
    Unavailable = 0,
    WildAnimal = 1,
    FarmAnimal = 2,
    ServiceAnimal = 3
};

using CpmVruSubClass = std::variant<
    CpmVruPedestrianSubClassCode,
    CpmVruBicyclistSubClassCode,
    CpmVruMotorcyclistSubClassCode,
    CpmVruAnimalSubClassCode>;

struct CpmVehicleObjectClass
{
    CpmVehicleSubClassCode subClass{CpmVehicleSubClassCode::Unknown};
};

struct CpmVruObjectClass
{
    CpmVruSubClass subClass{
        CpmVruPedestrianSubClassCode::Unavailable};
};

enum class CpmVruClusterProfile : std::uint8_t
{
    Pedestrian = 0,
    Bicyclist = 1,
    Motorcyclist = 2,
    Animal = 3
};

// ObjectClass.groupSubClass aplica WITH COMPONENTS
// { ..., clusterBoundingBoxShape ABSENT }. O campo não pertence
// a esta alternativa do perfil CPM R2.
struct CpmGroupObjectClass
{
    std::optional<std::uint8_t> clusterId;
    std::uint8_t clusterCardinalitySize{};
    std::optional<std::vector<CpmVruClusterProfile>> clusterProfiles;
};

enum class CpmOtherSubClassCode : std::uint8_t
{
    Unknown = 0,
    SingleObject = 1,
    MultipleObjects = 2,
    BulkMaterial = 3
};

struct CpmOtherObjectClass
{
    CpmOtherSubClassCode subClass{CpmOtherSubClassCode::Unknown};
};

// ObjectClass é uma escolha exclusiva entre quatro alternativas.
using CpmObjectClass = std::variant<
    CpmVehicleObjectClass,
    CpmVruObjectClass,
    CpmGroupObjectClass,
    CpmOtherObjectClass>;

// Tipo forte para uma razão de confiança normalizada no domínio neutro.
// Não representa a precisão física de CpmMeasuredValue::confidence.
// A validação de finitude/intervalo e a conversão para o domínio ASN.1
// pertencem ao mapper CPM R2.
struct CpmConfidenceRatio
{
    double value{};
};

// nullopt significa confiança indisponível; zero continua a ser um valor
// presente e distinto da indisponibilidade.
struct CpmObjectClassConfidence
{
    CpmObjectClass objectClass{CpmVehicleObjectClass{}};
    std::optional<CpmConfidenceRatio> confidenceRatio;
};

// Índices correspondentes aos componentes definidos em
// MatrixIncludedComponents no perfil CPM R2. São índices de bits, não
// valores ASN.1 gerados.
enum class CpmCorrelationComponent : std::size_t
{
    XPosition = 0,
    YPosition = 1,
    ZPosition = 2,
    XVelocityOrVelocityMagnitude = 3,
    YVelocityOrVelocityDirection = 4,
    ZSpeed = 5,
    XAccelerationOrMagnitude = 6,
    YAccelerationOrDirection = 7,
    ZAcceleration = 8,
    ZAngle = 9,
    YAngle = 10,
    XAngle = 11,
    ZAngularVelocity = 12
};

struct CpmCorrelationCellValue
{
    std::int16_t value{};

    constexpr CpmCorrelationCellValue() = default;

    constexpr CpmCorrelationCellValue(std::int16_t numericValue)
        : value(numericValue)
    {
    }

    static constexpr std::int16_t FullNegativeCorrelation = -100;
    static constexpr std::int16_t NoCorrelation = 0;
    static constexpr std::int16_t FullPositiveCorrelation = 100;
    static constexpr std::int16_t Unavailable = 101;
};

struct CpmCorrelationColumn
{
    std::vector<CpmCorrelationCellValue> values;
};

struct CpmCorrelationMatrix
{
    std::bitset<13> includedComponents;
    std::vector<CpmCorrelationColumn> columns;
};

struct CpmPerceivedObject
{
    // A identidade é obrigatória no modelo semântico interno, de acordo
    // com o guard definido na Etapa 10.2, embora o campo ASN.1 seja OPTIONAL.
    std::uint16_t objectId{};
    std::int32_t measurementDeltaTimeMs{};
    CpmPosition3D position{};
    std::optional<CpmVelocity3D> velocity;
    std::optional<CpmAcceleration3D> acceleration;
    std::optional<CpmObjectAngles> angles;
    std::optional<CpmAngularVelocityComponent> angularVelocity;
    std::optional<std::vector<CpmCorrelationMatrix>> correlationMatrices;
    std::optional<CpmObjectDimension> dimensionX;
    std::optional<CpmObjectDimension> dimensionY;
    std::optional<CpmObjectDimension> dimensionZ;
    std::optional<std::int32_t> objectAgeMs;
    std::optional<std::uint8_t> objectPerceptionQuality;
    std::optional<CpmMapPosition> mapPosition;
    std::optional<std::vector<std::uint8_t>> sensorIds;
    std::optional<std::vector<CpmObjectClassConfidence>> classifications;
};

struct CpmSensorInformation
{
    std::uint8_t sensorId{};
    CpmSensorKind sensorType{CpmSensorKind::Undefined};
    std::optional<CpmRegionShape> perceptionRegionShape;
    // Razão normalizada; nullopt significa confiança indisponível.
    std::optional<CpmConfidenceRatio> perceptionRegionConfidenceRatio;
    bool shadowingApplies{};
};

struct CpmPerceptionRegion
{
    std::int32_t measurementDeltaTimeMs{};
    // Razão normalizada; nullopt significa confiança indisponível.
    // O valor não está quantizado para o domínio inteiro ASN.1.
    std::optional<CpmConfidenceRatio> confidenceRatio;
    CpmRegionShape shape{CpmCircularShape{}};
    bool shadowingApplies{};
    std::optional<std::vector<std::uint8_t>> sensorIds;
    std::optional<std::uint8_t> numberOfPerceivedObjects;
    std::optional<std::vector<std::uint16_t>> perceivedObjectIds;
};

// Optional CPM R2 management-container metadata.
struct CpmMessageSegmentationInfo {
    std::uint8_t totalMsgNo{};
    std::uint8_t thisMsgNo{};
};

struct CpmMessageRateHz {
    std::uint8_t mantissa{};
    std::int8_t exponent{};
};

struct CpmMessageRateRange {
    CpmMessageRateHz messageRateMin{};
    CpmMessageRateHz messageRateMax{};
};

struct CpmSemanticData
{
    CpmStationData station{};
    std::int64_t referenceTimeItsMs{};
    CpmReferencePosition referencePosition{};
    std::optional<CpmMessageSegmentationInfo> segmentationInfo;
    std::optional<CpmMessageRateRange> messageRateRange;
    // Total global conhecido no LDM; distinto da contagem regional.
    // Não representa apenas os objetos incluídos na mensagem.
    std::uint8_t knownObjectCount{};
    std::vector<CpmPerceivedObject> perceivedObjects;
    std::vector<CpmSensorInformation> sensorInformation;
    std::vector<CpmPerceptionRegion> perceptionRegions;
};

#endif
