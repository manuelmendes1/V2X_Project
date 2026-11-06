#pragma once

#include <variant>

#include "vanetza_integration/vam/types/vam_identity_types.h"

namespace vanetza_integration
{
namespace vam
{

// Primeira origem funcional: uma SUMO Person associada
// a um nó ns-3 próprio e a uma fonte de mobilidade.
struct SumoPersonOrigin
{
    SumoPersonReference sumoPersonRef;
    Ns3NodeReference ns3NodeRef;
    MobilitySourceReference mobilitySourceRef;
};

// Origem prevista para uma ITS-S independente.
struct IndependentItsStationOrigin
{
    LogicalItsStationReference logicalStationRef;
    MobilitySourceReference mobilitySourceRef;
};

// A proveniência é resolvida na fronteira de integração.
// Não faz parte de VamSemanticData.
using VamOriginContext = std::variant<
    SumoPersonOrigin,
    IndependentItsStationOrigin>;

} // namespace vam
} // namespace vanetza_integration
