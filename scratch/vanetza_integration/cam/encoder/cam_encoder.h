#pragma once

#include <vector>

#include "vanetza_integration/cam/types/cam_input.h"
#include "vanetza_integration/context/packet/packet_context.h"

class CamEncoder {
public:

    // =========================================================================
    // Codificação ASN.1 (UPER)
    // -------------------------------------------------------------------------
    // Codifica uma estrutura lógica CamInput para o formato ASN.1 UPER.
    //
    // Responsabilidades:
    //   - solicitar a codificação ao wrapper Vanetza CAM;
    //   - executar a codificação UPER através da boundary Vanetza;
    //   - registar métricas e eventos de codificação.
    //
    // A validação dos dados de entrada é efetuada pelo CamValidator.
    // =========================================================================
    static std::vector<uint8_t> Encode(const ns3::PacketContext& ctx, const CamInput& input);
};
