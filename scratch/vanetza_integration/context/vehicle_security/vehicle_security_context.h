#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "vanetza_integration/context/vehicle_security/denm_credential.h"
#include "vanetza_integration/context/vehicle_security/replay/replay_context.h"
#include "vanetza_integration/denm/replay/denm_replay_context.h"

namespace ns3
{

// ============================================================================
// VehicleSecurityContext
// ----------------------------------------------------------------------------
// Estado de segurança permanente associado a um veículo.
//
// Este contexto agrega:
//
//   • material criptográfico (chave privada e certificado);
//
//   • cadeia de confiança;
//
//   • estado local de proteção contra replay.
//
// É o único proprietário ("owner") da chave privada e do certificado.
// ============================================================================
struct VehicleSecurityContext
{
    // ------------------------------------------------------------------------
    // Certificate status
    // ------------------------------------------------------------------------

    bool certLoaded = false;

    // ------------------------------------------------------------------------
    // Certificate files
    // ------------------------------------------------------------------------

    std::string certFile;
    std::string keyFile;

    // DENM credential pool discovered for this vehicle.
    std::vector<DenmCredential> denmCredentials;

    // Initial/active credential used by the current DENM signer.
    std::size_t activeDenmCredentialIndex = 0;

    // ------------------------------------------------------------------------
    // Cryptographic material
    // ------------------------------------------------------------------------

    EVP_PKEY* privateKey = nullptr;

    X509* certificate = nullptr;

    // ------------------------------------------------------------------------
    // Trust store
    // ------------------------------------------------------------------------

    std::vector<std::string> trustedCertificates;

    // ------------------------------------------------------------------------
    // Replay protection
    // ------------------------------------------------------------------------

    ReplayContext replay;
    DenmReplayContext denmReplay;

    // ------------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------------

    VehicleSecurityContext() = default;

    VehicleSecurityContext(const VehicleSecurityContext&) = delete;
    VehicleSecurityContext& operator=(const VehicleSecurityContext&) = delete;

    VehicleSecurityContext(VehicleSecurityContext&&) = delete;
    VehicleSecurityContext& operator=(VehicleSecurityContext&&) = delete;

    // ------------------------------------------------------------------------
    // DENM credential selection
    // ------------------------------------------------------------------------

    bool SetActiveDenmCredential(
        std::size_t index);

    bool RotateToNextDenmCredential();

    // ------------------------------------------------------------------------
    // Cleanup
    // ------------------------------------------------------------------------

    ~VehicleSecurityContext();
};

} // namespace ns3
