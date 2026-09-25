#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace ns3
{

// ============================================================================
// VehicleCredential
// ----------------------------------------------------------------------------
// Descritor explícito de uma credencial criptográfica pertencente a um
// veículo.
//
// Este tipo representa a identidade e a localização do material da
// credencial. Não possui nem gere EVP_PKEY/X509 e não executa I/O.
//
// Ownership do material criptográfico carregado permanece com
// VehicleSecurityContext. Loading/discovery continuam fora deste modelo.
// ============================================================================
struct VehicleCredential
{
    // Logical identifier of this credential.
    std::string credentialId;

    // Vehicle to which the credential is assigned.
    std::string vehicleId;

    // Monotonic credential sequence used by the PKI fixture.
    std::uint64_t sequence = 0;

    // Certificate and private-key locations.
    std::string certFile;
    std::string keyFile;

    // Compatibility constructor for the existing file-based contract.
    // Identity metadata can be populated by the discovery layer.
    VehicleCredential(
        const std::string& certificateFile,
        const std::string& privateKeyFile)
        :
        certFile(certificateFile),
        keyFile(privateKeyFile)
    {
    }

    VehicleCredential() = default;

    VehicleCredential(
        std::string id,
        std::string vehicle,
        std::uint64_t credentialSequence,
        std::string certificateFile,
        std::string privateKeyFile)
        :
        credentialId(std::move(id)),
        vehicleId(std::move(vehicle)),
        sequence(credentialSequence),
        certFile(std::move(certificateFile)),
        keyFile(std::move(privateKeyFile))
    {
    }

    bool IsUsable() const
    {
        return !certFile.empty() &&
               !keyFile.empty();
    }
};

} // namespace ns3
