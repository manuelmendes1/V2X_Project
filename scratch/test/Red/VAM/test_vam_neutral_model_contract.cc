#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "vanetza_integration/vam/types/vam_input.h"
#include "vanetza_integration/vam/types/vam_message.h"
#include "vanetza_integration/vam/context/vam_origin_context.h"
#include "vanetza_integration/vru/role/types/vru_role_types.h"

using namespace vanetza_integration::vam;
using vanetza_integration::vru::role::VruProfile;
using vanetza_integration::vru::role::VruRoleSnapshot;

template <typename T, typename = void>
struct HasReceivedAtUnixMs : std::false_type {};

template <typename T>
struct HasReceivedAtUnixMs<
    T, std::void_t<decltype(std::declval<T>().receivedAtUnixMs)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasOriginContext : std::false_type {};

template <typename T>
struct HasOriginContext<
    T, std::void_t<decltype(std::declval<T>().originContext)>>
    : std::true_type {};

// Input e mensagem são contratos distintos.
static_assert(
    !std::is_same_v<VamInput, VamMessage>,
    "VAM input and received message must be distinct types");

static_assert(
    std::is_base_of_v<VamSemanticData, VamInput>,
    "VAM input must expose shared semantic data");

static_assert(
    std::is_base_of_v<VamSemanticData, VamMessage>,
    "VAM message must expose shared semantic data");

// O timestamp de receção pertence apenas à mensagem recebida.
static_assert(
    !HasReceivedAtUnixMs<VamInput>::value,
    "VAM input must not contain a reception timestamp");

static_assert(
    HasReceivedAtUnixMs<VamMessage>::value,
    "VAM message must contain a reception timestamp");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamMessage&>().receivedAtUnixMs),
        std::int64_t>,
    "VAM reception timestamp must use signed 64-bit Unix milliseconds");

// O modelo reutiliza os tipos VRU existentes.
static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>().vruProfile),
        VruProfile>,
    "VAM must reuse the existing VruProfile type");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>().vruRole),
        VruRoleSnapshot>,
    "VAM must reuse the existing VruRoleSnapshot type");

// stationId usa um tipo próprio, não um inteiro genérico.
static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>()
                     .originatingStation.stationId),
        VamStationId>,
    "Station identity must use VamStationId");

// Os timestamps internos usam unidades explícitas.
static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>().generationTimeIts),
        std::chrono::microseconds>,
    "Generation time must use microseconds");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>().referencePositionTimeIts),
        std::chrono::microseconds>,
    "Reference position time must use microseconds");

// A posição preserva campos indisponíveis.
static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>()
                     .referencePosition.latitudeDeg),
        std::optional<double>>,
    "Reference latitude must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>()
                     .referencePosition.longitudeDeg),
        std::optional<double>>,
    "Reference longitude must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>()
                     .referencePosition.altitudeMeters),
        std::optional<double>>,
    "Reference altitude must preserve unavailable state");

// A cinemática também preserva campos indisponíveis.
static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>()
                     .highFrequencyState.headingDeg),
        std::optional<double>>,
    "Heading must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>()
                     .highFrequencyState.speedMps),
        std::optional<double>>,
    "Speed must preserve unavailable state");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamSemanticData&>()
                     .highFrequencyState.longitudinalAccelerationMps2),
        std::optional<double>>,
    "Acceleration must preserve unavailable state");

// A proveniência não pertence ao modelo semântico.
static_assert(
    !HasOriginContext<VamSemanticData>::value,
    "Origin context must remain outside VAM semantic data");

// Existem exatamente duas alternativas de origem.
static_assert(
    std::variant_size_v<VamOriginContext> == 2,
    "VAM must expose exactly two origin alternatives");

static_assert(
    std::is_same_v<
        std::variant_alternative_t<0, VamOriginContext>,
        SumoPersonOrigin>,
    "First origin must represent a SUMO Person");

static_assert(
    std::is_same_v<
        std::variant_alternative_t<1, VamOriginContext>,
        IndependentItsStationOrigin>,
    "Second origin must represent an independent ITS station");

// Cada referência conserva a sua categoria.
static_assert(
    std::is_same_v<
        decltype(std::declval<SumoPersonOrigin&>().sumoPersonRef),
        SumoPersonReference>);

static_assert(
    std::is_same_v<
        decltype(std::declval<SumoPersonOrigin&>().ns3NodeRef),
        Ns3NodeReference>);

static_assert(
    std::is_same_v<
        decltype(std::declval<SumoPersonOrigin&>().mobilitySourceRef),
        MobilitySourceReference>);

static_assert(
    std::is_same_v<
        decltype(std::declval<IndependentItsStationOrigin&>()
                     .logicalStationRef),
        LogicalItsStationReference>);

static_assert(
    std::is_same_v<
        decltype(std::declval<IndependentItsStationOrigin&>()
                     .mobilitySourceRef),
        MobilitySourceReference>);

// A referência de origem não se converte em identidade da estação.
static_assert(
    !std::is_convertible_v<SumoPersonReference, VamStationId>);

static_assert(
    !std::is_convertible_v<Ns3NodeReference, VamStationId>);

static_assert(
    !std::is_convertible_v<MobilitySourceReference, VamStationId>);

static_assert(
    !std::is_convertible_v<LogicalItsStationReference, VamStationId>);

int main()
{
    VamInput input{};

    input.originatingStation.stationId = VamStationId{1001};
    input.vruProfile = VruProfile::Pedestrian;

    input.generationTimeIts = std::chrono::microseconds{2000000};
    input.referencePositionTimeIts = std::chrono::microseconds{1995000};

    input.referencePosition.latitudeDeg = 41.0;
    input.referencePosition.longitudeDeg = -8.0;
    input.highFrequencyState.speedMps = 1.2;

    VamMessage message{};
    message.receivedAtUnixMs = 1793952000000LL;

    VamOriginContext sumoOrigin = SumoPersonOrigin{
        SumoPersonReference{"person-42"},
        Ns3NodeReference{"node-7"},
        MobilitySourceReference{"sumo-mobility-42"}
    };

    const auto* person = std::get_if<SumoPersonOrigin>(&sumoOrigin);

    if (!person ||
        person->sumoPersonRef.value != "person-42" ||
        person->ns3NodeRef.value != "node-7" ||
        person->mobilitySourceRef.value != "sumo-mobility-42")
    {
        return 1;
    }

    VamOriginContext independentOrigin = IndependentItsStationOrigin{
        LogicalItsStationReference{"logical-station-9"},
        MobilitySourceReference{"independent-mobility-9"}
    };

    const auto* independent =
        std::get_if<IndependentItsStationOrigin>(&independentOrigin);

    if (!independent ||
        independent->logicalStationRef.value != "logical-station-9" ||
        independent->mobilitySourceRef.value != "independent-mobility-9")
    {
        return 2;
    }

    if (input.originatingStation.stationId.value != 1001 ||
        message.receivedAtUnixMs != 1793952000000LL)
    {
        return 3;
    }

    return 0;
}
