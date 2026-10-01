#include "vanetza_integration/cam/builder/cam_builder.h"
#include "vanetza_integration/security/cam_security.h"
#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <vanetza/security/v3/persistence.hpp>

#include "vanetza_integration/context/vehicle_security/vehicle_credential_binding_validator.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{

int failures = 0;

void
Expect(bool condition, const std::string& message)
{
    if (condition)
    {
        std::cout << "PASS " << message << '\n';
    }
    else
    {
        std::cout << "FAIL " << message << '\n';
        ++failures;
    }
}

EVP_PKEY*
LoadPrivateKey(const std::string& path)
{
    FILE* file = std::fopen(path.c_str(), "rb");

    if (file == nullptr)
    {
        return nullptr;
    }

    EVP_PKEY* key =
        PEM_read_PrivateKey(
            file,
            nullptr,
            nullptr,
            nullptr);

    std::fclose(file);

    return key;
}

} // namespace

int
main()
{
    std::cout
        << "===== CAM DENM COMMON CREDENTIAL PATH TEST =====\n";

    unsetenv("V2X_DENM_PKI_DIR");

    const std::string pkiDir =
        "/home/mmendes/vanetza-pki-lab/validation-denm-pki";

    const std::string expectedCert =
        pkiDir + "/veh0-ticket-000.cert";

    const std::string expectedKey =
        pkiDir + "/veh0-ticket-000.pem";

    const std::string expectedCertRotated =
        pkiDir + "/veh0-ticket-001.cert";

    const std::string expectedKeyRotated =
        pkiDir + "/veh0-ticket-001.pem";

    ns3::SimulationController::SetDenmPkiDirectory(
        pkiDir);

    ns3::VehicleState vehicle;

    vehicle.identity.vehId = "veh0";
    vehicle.identity.nodeId = 0;
    vehicle.identity.stationId = 1;

    ns3::VehicleSecurityInitializer::Initialize(
        vehicle);

    const auto* activeCredential =
        vehicle.security.credentialPool.GetActiveCredential();

    Expect(
        activeCredential != nullptr,
        "active credential exists");

    Expect(
        activeCredential != nullptr &&
            activeCredential->credentialId ==
                "veh0-ticket-000",
        "active credential is veh0-ticket-000");

    Expect(
        activeCredential != nullptr &&
            activeCredential->certFile == expectedCert,
        "active credential selects expected certificate");

    Expect(
        activeCredential != nullptr &&
            activeCredential->keyFile == expectedKey,
        "active credential selects expected private key");

    Expect(
        vehicle.security.certFile == expectedCert,
        "VehicleSecurityContext exposes active certificate path");

    Expect(
        vehicle.security.keyFile == expectedKey,
        "VehicleSecurityContext exposes active private key path");

    ns3::CamGenerationContext cam =
        ns3::CamBuilder::Build(vehicle);

    Expect(
        cam.security.privateKey ==
            vehicle.security.privateKey,
        "CAM context references VehicleSecurityContext private key");

    Expect(
        cam.security.certificate ==
            vehicle.security.certificate,
        "CAM context references VehicleSecurityContext certificate");

    EVP_PKEY* expectedPrivateKey =
        LoadPrivateKey(expectedKey);

    Expect(
        expectedPrivateKey != nullptr,
        "active credential private key can be loaded by OpenSSL");

    if (expectedPrivateKey != nullptr &&
        cam.security.privateKey != nullptr)
    {
        const int keyMatch =
            EVP_PKEY_eq(
                cam.security.privateKey,
                expectedPrivateKey);

        Expect(
            keyMatch == 1,
            "CAM private key is the active credential private key");
    }

    EVP_PKEY* camPublicKey = nullptr;

    if (vehicle.security.certificate != nullptr)
    {
        camPublicKey =
            X509_get_pubkey(
                vehicle.security.certificate);
    }

    Expect(
        camPublicKey != nullptr,
        "CAM certificate exposes a public key");

    if (expectedPrivateKey != nullptr &&
        camPublicKey != nullptr)
    {
        const int publicKeyMatch =
            EVP_PKEY_eq(
                camPublicKey,
                expectedPrivateKey);

        Expect(
            publicKeyMatch == 1,
            "CAM certificate is bound to the active credential key");
    }

    ns3::PacketContext context;

    context.senderVehId =
        vehicle.identity.vehId;

    context.senderNodeId =
        vehicle.identity.nodeId;

    context.senderStationId =
        vehicle.identity.stationId;

    const std::vector<uint8_t> payload{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05
    };

    const auto signature =
        CamSecurity::Sign(
            context,
            payload,
            cam.security.privateKey);

    Expect(
        !signature.empty(),
        "CAM signing produces a signature");

    if (!signature.empty() &&
        vehicle.security.certificate != nullptr)
    {
        const bool verified =
            CamSecurity::Verify(
                context,
                payload,
                signature,
                vehicle.security.certificate);

        Expect(
            verified,
            "CAM signature verifies with its bound certificate");
    }

    // ---------------------------------------------------------------------
    // Credential rotation must update the active credential actually
    // consumed by CAM, including its cached cryptographic material.
    // ---------------------------------------------------------------------

    const bool rotationSucceeded =
        vehicle.security.RotateToNextDenmCredential();

    Expect(
        rotationSucceeded,
        "credential rotation to ticket-001 succeeds");

    const auto* rotatedCredential =
        vehicle.security.credentialPool.GetActiveCredential();

    Expect(
        rotatedCredential != nullptr &&
            rotatedCredential->credentialId ==
                "veh0-ticket-001",
        "ticket-001 becomes the active credential after rotation");

    Expect(
        vehicle.security.certFile == expectedCertRotated,
        "VehicleSecurityContext exposes rotated certificate path");

    Expect(
        vehicle.security.keyFile == expectedKeyRotated,
        "VehicleSecurityContext exposes rotated private-key path");

    EVP_PKEY* rotatedPrivateKey =
        LoadPrivateKey(expectedKeyRotated);

    Expect(
        rotatedPrivateKey != nullptr,
        "rotated credential private key can be loaded by OpenSSL");

    ns3::CamGenerationContext camAfterRotation =
        ns3::CamBuilder::Build(vehicle);

    Expect(
        camAfterRotation.security.privateKey != nullptr,
        "CAM context has a private key after credential rotation");

    if (rotatedPrivateKey != nullptr &&
        camAfterRotation.security.privateKey != nullptr)
    {
        const int rotatedKeyMatch =
            EVP_PKEY_eq(
                camAfterRotation.security.privateKey,
                rotatedPrivateKey);

        Expect(
            rotatedKeyMatch == 1,
            "CAM private key follows the rotated active credential");
    }

    const ns3::VehicleCredential expectedRotatedCredential{
        "veh0-ticket-001",
        "veh0",
        1,
        expectedCertRotated,
        expectedKeyRotated
    };

    const bool rotatedCredentialBinding =
        ns3::VehicleCredentialBindingValidator::Validate(
            expectedRotatedCredential);

    Expect(
        rotatedCredentialBinding,
        "rotated credential certificate and private key are cryptographically bound");

    bool rotatedCertificateLoaded = false;

    try
    {
        const auto rotatedCertificate =
            vanetza::security::v3::load_certificate_from_file(
                expectedCertRotated);

        rotatedCertificateLoaded =
            rotatedCertificate.content() != nullptr;
    }
    catch (...)
    {
        rotatedCertificateLoaded = false;
    }

    Expect(
        rotatedCertificateLoaded,
        "rotated V3 credential certificate can be loaded by Vanetza");

    EVP_PKEY* camAfterRotationPublicKey = nullptr;

    if (camAfterRotation.security.certificate != nullptr)
    {
        camAfterRotationPublicKey =
            X509_get_pubkey(
                camAfterRotation.security.certificate);
    }

    Expect(
        camAfterRotationPublicKey != nullptr,
        "CAM context exposes a certificate public key after rotation");

    if (rotatedPrivateKey != nullptr &&
        camAfterRotationPublicKey != nullptr)
    {
        const int rotatedCertificateMatch =
            EVP_PKEY_eq(
                camAfterRotationPublicKey,
                rotatedPrivateKey);

        Expect(
            rotatedCertificateMatch == 1,
            "CAM certificate follows the rotated active credential");
    }

    const auto rotatedSignature =
        CamSecurity::Sign(
            context,
            payload,
            camAfterRotation.security.privateKey);

    Expect(
        !rotatedSignature.empty(),
        "CAM signing still produces a signature after rotation");

    if (camAfterRotationPublicKey != nullptr)
    {
        EVP_PKEY_free(camAfterRotationPublicKey);
    }

    if (rotatedPrivateKey != nullptr)
    {
        EVP_PKEY_free(rotatedPrivateKey);
    }

    if (camPublicKey != nullptr)
    {
        EVP_PKEY_free(camPublicKey);
    }

    if (expectedPrivateKey != nullptr)
    {
        EVP_PKEY_free(expectedPrivateKey);
    }

    std::cout
        << "CAM_DENM_COMMON_CREDENTIAL_PATH_EXIT="
        << failures
        << '\n';

    return failures;
}
