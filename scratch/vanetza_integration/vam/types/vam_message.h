#pragma once

#include <cstdint>

#include "vanetza_integration/vam/types/vam_domain_types.h"

namespace vanetza_integration
{
namespace vam
{

struct VamMessage : VamSemanticData
{
    // Instante Unix de receção local. Não é o instante
    // de geração nem o timestamp de referência da posição.
    std::int64_t receivedAtUnixMs{};
};

} // namespace vam
} // namespace vanetza_integration
