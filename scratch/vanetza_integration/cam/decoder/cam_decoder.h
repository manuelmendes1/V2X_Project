#pragma once

#include <vector>
#include <cstdint>

#include "vanetza_integration/cam/types/cam_message.h"
#include "vanetza_integration/context/packet/packet_context.h"

class CamDecoder {

public:

    // =========================================================================
    // Decodificação de uma CAM ASN.1
    //
    // Recebe o contexto do recetor para logging e rastreabilidade.
    // =========================================================================
    static CamMessage Decode(const ns3::PacketContext& ctx, const std::vector<uint8_t>& rawPacket);
};
