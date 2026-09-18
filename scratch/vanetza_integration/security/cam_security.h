#ifndef CAM_SECURITY_H
#define CAM_SECURITY_H

#include <cstdint>
#include <vector>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "vanetza_integration/context/packet/packet_context.h"

class CamSecurity{

public:

    // ======================================================
    // Assinatura Digital
    // ======================================================

    static std::vector<uint8_t> Sign(const ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, EVP_PKEY* privateKey);

    // ======================================================
    // Verificação da assinatura
    // ======================================================

    static bool Verify(const ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, const std::vector<uint8_t>& signature, X509* certificate);
};

#endif
