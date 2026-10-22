#ifndef VANETZA_INTEGRATION_CPM_CODEC_CPM_UPER_ENCODER_H
#define VANETZA_INTEGRATION_CPM_CODEC_CPM_UPER_ENCODER_H

#include "vanetza_integration/cpm/types/cpm_input.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct CpmUperEncodeResult
{
    bool success = false;
    std::vector<std::uint8_t> bytes;
    std::size_t encodedBits = 0;
    std::string error;
};

class CpmUperEncoder
{
public:
    // Codifica o perfil CPM R2 de origem RSU atualmente suportado.
    // Campos sem mapper implementado são rejeitados explicitamente.
    static CpmUperEncodeResult Encode(const CpmInput& input);
};

#endif
