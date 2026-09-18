#pragma once

#include "vanetza_integration/cam/types/cam_message.h"

extern "C" {

#include "vanetza/asn1/its/CAM.h"

}

class CamConverter {

public:

    // =========================================================================
    // Conversão de uma CAM ASN.1 para a representação interna.
    // =========================================================================
    static CamMessage ToMessage(const CAM_t& cam, uint64_t timestamp);
};
