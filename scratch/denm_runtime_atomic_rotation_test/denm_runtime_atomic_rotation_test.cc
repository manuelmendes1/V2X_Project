#include <openssl/evp.h>
#include <openssl/x509.h>

#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"

#include "vanetza_integration/tests/denm_test_pki.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-module.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace
{

bool
Check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }

    std::cout << "PASS " << message << '\n';
    return true;
}

} // namespace

int
main()
{
    std::cout
        << "===== DENM RUNTIME ATOMIC ROTATION TEST =====\n";

    const std::string sourcePki =
        ns3::denm_test::ResolveDenmPkiDirectory();

    if (sourcePki.empty())
    {
        std::cerr
            << "FAIL DENM test PKI directory unavailable\n";
        return 1;
    }

    const fs::path testPki =
        fs::temp_directory_path() /
        "denm_runtime_atomic_rotation_test";

    std::error_code ec;
    fs::remove_all(testPki, ec);
    fs::create_directories(testPki, ec);

    if (ec)
    {
        std::cerr
            << "FAIL could not create temporary PKI: "
            << ec.message() << '\n';
        return 1;
    }

    /*
     * Start from a real valid veh0 fixture.
     */
    for (const char* file : {
             "denm-root.cert",
             "denm-aa.cert",
             "aa.key",
             "veh0-ticket-000.cert",
             "veh0-ticket-000.pem",
             "veh0-ticket-001.pem"})
    {
        fs::copy_file(
            fs::path(sourcePki) / file,
            testPki / file,
            fs::copy_options::overwrite_existing,
            ec);

        if (ec)
        {
            std::cerr
                << "FAIL copying "
                << file
                << ": "
                << ec.message()
                << '\n';
            return 1;
        }
    }

    /*
     * Deliberately make credential 001 unusable while keeping the
     * expected filename present. The credential 001 certificate will
     * still exist, but the private key/certificate binding is invalid
     * because 001 certificate is paired with 000 key.
     */
    fs::copy_file(
        fs::path(sourcePki) / "veh0-ticket-001.cert",
        testPki / "veh0-ticket-001.cert",
        fs::copy_options::overwrite_existing,
        ec);

    if (ec)
    {
        std::cerr
            << "FAIL copying invalid target certificate: "
            << ec.message()
            << '\n';
        return 1;
    }

    /*
     * Replace ticket 001 PEM with ticket 000 PEM so the target
     * credential fails binding validation.
     */
    fs::copy_file(
        fs::path(sourcePki) / "veh0-ticket-000.pem",
        testPki / "veh0-ticket-001.pem",
        fs::copy_options::overwrite_existing,
        ec);

    if (ec)
    {
        std::cerr
            << "FAIL creating mismatched target key: "
            << ec.message()
            << '\n';
        return 1;
    }

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        testPki.string());

    SimulationContext::itsStartTime =
        "2026-09-24T13:38:21";

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();

    auto channel =
        ns3::YansWifiChannelHelper::Default().Create();

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase(
        "10.0.0.0",
        "255.255.255.0");

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        ns3::MakeNullCallback<
            void,
            ns3::Ptr<ns3::Socket>>());

    /*
     * Vehicle creation itself may reject an invalid credential set,
     * therefore this RED test first proves the fixture contains the
     * intended two credentials.
     */
    ns3::VehicleFactory::CreateVehicle("veh0");

    ns3::VehicleState* vehicle =
        registry.GetByVehId("veh0");

    bool ok = true;

    ok &= Check(
        vehicle != nullptr,
        "veh0 is created through the real VehicleFactory");

    if (vehicle == nullptr)
    {
        return 1;
    }

    ok &= Check(
        vehicle->runtime != nullptr &&
            vehicle->runtime->IsInitialized(),
        "VanetzaRuntime is initialized");

    ok &= Check(
        vehicle->security.credentialPool.Size() >= 2,
        "veh0 exposes at least two credentials");

    ok &= Check(
        vehicle->security.credentialPool.GetActiveCredentialIndex() == 0,
        "credential 000 is initially active");

    ok &= Check(
        vehicle->security.certFile.ends_with(
            "veh0-ticket-000.cert"),
        "credential 000 certificate is initially active");

    const std::string originalCert =
        vehicle->security.certFile;

    const std::string originalKey =
        vehicle->security.keyFile;

    EVP_PKEY* originalPrivateKey =
        vehicle->security.privateKey != nullptr
            ? EVP_PKEY_dup(vehicle->security.privateKey)
            : nullptr;

    EVP_PKEY* originalCertificatePublicKey =
        vehicle->security.certificate != nullptr
            ? X509_get_pubkey(vehicle->security.certificate)
            : nullptr;

    ok &= Check(
        originalPrivateKey != nullptr,
        "original private key is available for atomic-rollback comparison");

    ok &= Check(
        originalCertificatePublicKey != nullptr,
        "original certificate public key is available for atomic-rollback comparison");

    /*
     * The core P10 assertion: the runtime rotation must fail atomically
     * and preserve every externally visible security-state field.
     */
    ok &= Check(
        !vehicle->runtime->RotateDenmCredential(),
        "runtime rotation to invalid credential is rejected");

    ok &= Check(
        vehicle->security.credentialPool.GetActiveCredentialIndex() == 0,
        "failed rotation preserves active credential index");

    ok &= Check(
        vehicle->security.certFile == originalCert,
        "failed rotation preserves certificate path");

    ok &= Check(
        vehicle->security.keyFile == originalKey,
        "failed rotation preserves private-key path");

    ok &= Check(
        vehicle->security.privateKey != nullptr &&
            originalPrivateKey != nullptr &&
            EVP_PKEY_eq(
                vehicle->security.privateKey,
                originalPrivateKey) == 1,
        "failed rotation preserves private-key cryptographic identity");

    EVP_PKEY* restoredCertificatePublicKey =
        vehicle->security.certificate != nullptr
            ? X509_get_pubkey(vehicle->security.certificate)
            : nullptr;

    ok &= Check(
        restoredCertificatePublicKey != nullptr &&
            originalCertificatePublicKey != nullptr &&
            EVP_PKEY_eq(
                restoredCertificatePublicKey,
                originalCertificatePublicKey) == 1,
        "failed rotation preserves certificate public-key identity");

    if (restoredCertificatePublicKey != nullptr)
    {
        EVP_PKEY_free(restoredCertificatePublicKey);
    }

    if (originalCertificatePublicKey != nullptr)
    {
        EVP_PKEY_free(originalCertificatePublicKey);
    }

    if (originalPrivateKey != nullptr)
    {
        EVP_PKEY_free(originalPrivateKey);
    }

    registry.Clear();

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory("");
    SimulationContext::itsStartTime.clear();

    fs::remove_all(testPki, ec);

    const int rc = ok ? 0 : 1;

    std::cout
        << "DENM_RUNTIME_ATOMIC_ROTATION_EXIT="
        << rc
        << '\n';

    return rc;
}
