#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"

#include "ns3/callback.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ptr.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-module.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include "vanetza_integration/tests/denm_test_pki.h"

namespace
{

bool
Check(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << "\n";
        return false;
    }

    std::cout << "PASS " << message << "\n";
    return true;
}

} // namespace

int
main()
{
    std::cout
        << "===== DENM RUNTIME CREDENTIAL ROTATION INTEGRATION TEST =====\n";

    const std::string pkiDir =
        ns3::denm_test::ResolveDenmPkiDirectory();

    if (pkiDir.empty())
    {
        std::cerr
            << "FAIL DENM test PKI directory is unavailable; "
               "set DENM_TEST_PKI_DIR or provide "
               "$HOME/vanetza-pki-lab/validation-denm-pki\n";
        return 1;
    }

    bool ok = true;

    // Configure the same explicit DENM PKI directory used by the
    // VehicleSecurityInitializer. DENM_TEST_PKI_DIR is only the
    // test environment variable; it is not consumed by production code.
    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        pkiDir);

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
        ns3::MakeNullCallback<void, ns3::Ptr<ns3::Socket>>());

    ns3::VehicleFactory::CreateVehicle("veh0");

    ns3::VehicleState* vehicle =
        registry.GetByVehId("veh0");

    ok &= Check(
        vehicle != nullptr,
        "veh0 is created through the real VehicleFactory");

    if (vehicle == nullptr)
    {
        std::cout
            << "DENM_RUNTIME_CREDENTIAL_ROTATION_EXIT=1\n";
        return 1;
    }

    ok &= Check(
        vehicle->runtime != nullptr,
        "veh0 owns an initialized VanetzaRuntime");

    ok &= Check(
        vehicle->runtime != nullptr &&
            vehicle->runtime->IsInitialized(),
        "VanetzaRuntime is initialized");

    ok &= Check(
        vehicle->security.denmCredentialPool.Size() >= 2,
        "veh0 has at least two DENM credentials");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredentialIndex() == 0,
        "credential 000 is active before runtime rotation");

    ok &= Check(
        vehicle->security.certFile.ends_with(
            "veh0-ticket-000.cert"),
        "credential 000 certificate is active before rotation");

    const std::size_t credentialCount =
        vehicle->security.denmCredentialPool.Size();

    ok &= Check(
        credentialCount >= 2,
        "credential pool provides at least one rotation target");

    for (std::size_t expectedIndex = 1;
         expectedIndex < credentialCount;
         ++expectedIndex)
    {
        ok &= Check(
            vehicle->runtime != nullptr &&
                vehicle->runtime->RotateDenmCredential(),
            "runtime rotation advances to credential " +
                std::to_string(expectedIndex));

        ok &= Check(
            vehicle->security.denmCredentialPool.GetActiveCredentialIndex() ==
                expectedIndex,
            "credential " +
                std::to_string(expectedIndex) +
                " becomes active through runtime rotation");

        if (expectedIndex < credentialCount)
        {
            ok &= Check(
                vehicle->security.certFile ==
                    vehicle->security.denmCredentialPool.GetCredential(expectedIndex)->certFile,
                "active certificate matches credential " +
                    std::to_string(expectedIndex));
        }
    }

    const std::size_t lastCredentialIndex =
        credentialCount - 1;

    ok &= Check(
        vehicle->runtime != nullptr &&
            !vehicle->runtime->RotateDenmCredential(),
        "rotation beyond the available credential pool is rejected");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredentialIndex() ==
            lastCredentialIndex,
        "rejected runtime rotation preserves the last active credential");

    registry.Clear();

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory("");

    std::cout
        << "DENM_RUNTIME_CREDENTIAL_ROTATION_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
