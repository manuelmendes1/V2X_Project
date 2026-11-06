#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "vanetza_integration/vam/types/vam_identity_types.h"
#include "vanetza_integration/vam/types/vam_domain_types.h"
#include "vanetza_integration/vam/context/vam_origin_context.h"

using namespace vanetza_integration::vam;

template <typename T, typename = void>
struct HasGnAddress : std::false_type {};

template <typename T>
struct HasGnAddress<
    T, std::void_t<decltype(std::declval<T>().gnAddress)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasLinkMac : std::false_type {};

template <typename T>
struct HasLinkMac<
    T, std::void_t<decltype(std::declval<T>().linkMac)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasPacketId : std::false_type {};

template <typename T>
struct HasPacketId<
    T, std::void_t<decltype(std::declval<T>().packetId)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasCertificatePseudonym : std::false_type {};

template <typename T>
struct HasCertificatePseudonym<
    T, std::void_t<decltype(std::declval<T>().certificatePseudonym)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasSumoPersonId : std::false_type {};

template <typename T>
struct HasSumoPersonId<
    T, std::void_t<decltype(std::declval<T>().sumoPersonId)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasVehicleId : std::false_type {};

template <typename T>
struct HasVehicleId<
    T, std::void_t<decltype(std::declval<T>().vehicleId)>>
    : std::true_type {};

// stationId é uma identidade de estação, não um inteiro genérico.
static_assert(
    std::is_same_v<
        decltype(std::declval<VamOriginatingStation&>().stationId),
        VamStationId>,
    "stationId must use the dedicated VamStationId type");

static_assert(
    std::is_same_v<
        decltype(std::declval<VamStationId&>().value),
        std::uint32_t>,
    "VamStationId must preserve its explicit 32-bit representation");

static_assert(
    !std::is_convertible_v<VamStationId, std::uint32_t>,
    "VamStationId must not implicitly convert to an integer");

static_assert(
    !std::is_convertible_v<std::uint32_t, VamStationId>,
    "An integer must not implicitly become a VamStationId");

// Cada referência de origem tem uma categoria própria.
static_assert(
    !std::is_same_v<VamStationId, SumoPersonReference>);
static_assert(
    !std::is_same_v<VamStationId, Ns3NodeReference>);
static_assert(
    !std::is_same_v<VamStationId, MobilitySourceReference>);
static_assert(
    !std::is_same_v<VamStationId, LogicalItsStationReference>);

static_assert(
    !std::is_same_v<SumoPersonReference, Ns3NodeReference>);
static_assert(
    !std::is_same_v<SumoPersonReference, MobilitySourceReference>);
static_assert(
    !std::is_same_v<SumoPersonReference, LogicalItsStationReference>);

static_assert(
    !std::is_same_v<Ns3NodeReference, MobilitySourceReference>);
static_assert(
    !std::is_same_v<Ns3NodeReference, LogicalItsStationReference>);

static_assert(
    !std::is_same_v<MobilitySourceReference, LogicalItsStationReference>);

// As categorias também não podem ser convertidas implicitamente.
static_assert(
    !std::is_convertible_v<SumoPersonReference, VamStationId>);
static_assert(
    !std::is_convertible_v<Ns3NodeReference, VamStationId>);
static_assert(
    !std::is_convertible_v<MobilitySourceReference, VamStationId>);
static_assert(
    !std::is_convertible_v<LogicalItsStationReference, VamStationId>);

static_assert(
    !std::is_convertible_v<SumoPersonReference, Ns3NodeReference>);
static_assert(
    !std::is_convertible_v<Ns3NodeReference, MobilitySourceReference>);
static_assert(
    !std::is_convertible_v<MobilitySourceReference, LogicalItsStationReference>);

// Verificar a tipagem de cada referência nas duas origens.
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
        decltype(std::declval<IndependentItsStationOrigin&>().logicalStationRef),
        LogicalItsStationReference>);

static_assert(
    std::is_same_v<
        decltype(std::declval<IndependentItsStationOrigin&>().mobilitySourceRef),
        MobilitySourceReference>);

// Identidades de rede, acesso, tracing e segurança não pertencem
// ao modelo semântico VAM.
static_assert(!HasGnAddress<VamSemanticData>::value);
static_assert(!HasLinkMac<VamSemanticData>::value);
static_assert(!HasPacketId<VamSemanticData>::value);
static_assert(!HasCertificatePseudonym<VamSemanticData>::value);
static_assert(!HasSumoPersonId<VamSemanticData>::value);
static_assert(!HasVehicleId<VamSemanticData>::value);


// VAM_IDENTITY_MATRIX_ASSERTIONS
// Todas as categorias de identidade devem permanecer distintas
// e não convertíveis implicitamente entre si.
template <typename A, typename B>
inline constexpr bool AreDistinctAndNonConvertible =
    !std::is_same_v<A, B> &&
    !std::is_convertible_v<A, B> &&
    !std::is_convertible_v<B, A>;

static_assert(AreDistinctAndNonConvertible<VamStationId, SumoPersonReference>);
static_assert(AreDistinctAndNonConvertible<VamStationId, Ns3NodeReference>);
static_assert(AreDistinctAndNonConvertible<VamStationId, MobilitySourceReference>);
static_assert(AreDistinctAndNonConvertible<VamStationId, LogicalItsStationReference>);

static_assert(AreDistinctAndNonConvertible<SumoPersonReference, Ns3NodeReference>);
static_assert(AreDistinctAndNonConvertible<SumoPersonReference, MobilitySourceReference>);
static_assert(AreDistinctAndNonConvertible<SumoPersonReference, LogicalItsStationReference>);

static_assert(AreDistinctAndNonConvertible<Ns3NodeReference, MobilitySourceReference>);
static_assert(AreDistinctAndNonConvertible<Ns3NodeReference, LogicalItsStationReference>);

static_assert(AreDistinctAndNonConvertible<MobilitySourceReference, LogicalItsStationReference>);

// Perfil e papel VRU são atributos funcionais, não identidades ITS.
static_assert(!std::is_same_v<
    VamStationId,
    vanetza_integration::vru::role::VruProfile>);
static_assert(!std::is_convertible_v<
    VamStationId,
    vanetza_integration::vru::role::VruProfile>);
static_assert(!std::is_convertible_v<
    vanetza_integration::vru::role::VruProfile,
    VamStationId>);
static_assert(!std::is_convertible_v<
    vanetza_integration::vru::role::VruRoleSnapshot,
    VamStationId>);
static_assert(!std::is_convertible_v<
    VamStationId,
    vanetza_integration::vru::role::VruRoleSnapshot>);

// Referências de integração não devem contaminar o modelo semântico.
template <typename T, typename = void>
struct HasSumoPersonReferenceField : std::false_type {};

template <typename T>
struct HasSumoPersonReferenceField<
    T, std::void_t<decltype(std::declval<T>().sumoPersonRef)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasNs3NodeReferenceField : std::false_type {};

template <typename T>
struct HasNs3NodeReferenceField<
    T, std::void_t<decltype(std::declval<T>().ns3NodeRef)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasMobilitySourceReferenceField : std::false_type {};

template <typename T>
struct HasMobilitySourceReferenceField<
    T, std::void_t<decltype(std::declval<T>().mobilitySourceRef)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasLogicalItsStationReferenceField : std::false_type {};

template <typename T>
struct HasLogicalItsStationReferenceField<
    T, std::void_t<decltype(std::declval<T>().logicalStationRef)>>
    : std::true_type {};

static_assert(!HasSumoPersonReferenceField<VamSemanticData>::value);
static_assert(!HasNs3NodeReferenceField<VamSemanticData>::value);
static_assert(!HasMobilitySourceReferenceField<VamSemanticData>::value);
static_assert(!HasLogicalItsStationReferenceField<VamSemanticData>::value);

int main()
{
    const VamStationId station{9001};
    const SumoPersonReference person{"person-42"};
    const Ns3NodeReference node{"node-7"};
    const MobilitySourceReference mobility{"mobility-42"};
    const LogicalItsStationReference logicalStation{"its-station-9"};

    if (station.value != 9001 ||
        person.value != "person-42" ||
        node.value != "node-7" ||
        mobility.value != "mobility-42" ||
        logicalStation.value != "its-station-9")
    {
        return 1;
    }

    return 0;
}
