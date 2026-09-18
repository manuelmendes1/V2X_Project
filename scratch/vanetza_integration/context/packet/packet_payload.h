#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "packet_types.h"

namespace ns3
{

// ============================================================================
// PacketPayload
//
// Representa o payload associado a um PacketContext.
// O conteúdo permanece em binário (bytes); a conversão para hexadecimal
// é efetuada apenas quando necessária para logging/debug.
// ============================================================================

struct PacketPayload
{
    //----------------------------------------------------------------------
    // Tipo do payload
    //----------------------------------------------------------------------

    PayloadType type = PayloadType::NONE;

    //----------------------------------------------------------------------
    // Conteúdo
    //----------------------------------------------------------------------

    std::vector<uint8_t> bytes;

    //----------------------------------------------------------------------
    // Informação adicional
    //----------------------------------------------------------------------

    uint32_t signatureSize = 0;

    //----------------------------------------------------------------------
    // Operações
    //----------------------------------------------------------------------

    bool Empty() const;

    std::size_t Size() const;

    void Clear();

    std::string ToHex(std::size_t maxBytes = 64) const;
};

} // namespace ns3
