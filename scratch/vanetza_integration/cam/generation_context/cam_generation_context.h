#pragma once

#include <memory>

#include <openssl/evp.h>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/cam/security_context/cam_security_context.h"
#include "vanetza_integration/cam/types/cam_input.h"

namespace ns3 {

    struct CamGenerationContext {

        std::shared_ptr<PacketContext> packetContext;

        CamInput camInput;

        CamSecurityContext security;
    };

} // namespace ns3
