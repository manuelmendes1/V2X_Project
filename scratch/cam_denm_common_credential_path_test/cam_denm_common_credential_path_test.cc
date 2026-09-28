#include "vanetza_integration/cam/builder/cam_builder.h"
#include "vanetza_integration/security/cam_security.h"
#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

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
        << "===== CAM DENM COMMON CREDENTIAL PATH RED TEST =====\n";

    unsetenv("V2X_DENM_PKI_DIR");

    const std::string pkiDir =
        "/home/mmendes/vanetza-pki-lab/validation-denm-pki";

    const std::string expectedCert =
        pkiDir + "/veh0-ticket-000.cert";

    const std::string expectedKey =
        pkiDir + "/veh0-ticket-000.pem";

    ns3::SimulationController::SetDenmPkiDirectory(
        pkiDir);

    ns3::VehicleState vehicle;

    vehicle.identity.vehId = "veh0";
    vehicle.identity.nodeId = 0;
    vehicle.identity.stationId = 1;

    ns3::VehicleSecurityInitializer::Initialize(
        vehicle);

    const auto* activeCredential =
        vehicle.security.denmCredentialPool.GetActiveCredential();

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
