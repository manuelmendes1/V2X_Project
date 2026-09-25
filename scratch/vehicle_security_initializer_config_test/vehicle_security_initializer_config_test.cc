#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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

} // namespace

int
main()
{
    std::cout << "===== VEHICLE SECURITY PKI CONFIG CONTROLLER TEST =====\n";

    unsetenv("V2X_DENM_PKI_DIR");

    ns3::VehicleState state;
    state.identity.vehId = "veh0";

    const std::string pkiDir =
        "/home/mmendes/vanetza-pki-lab/validation-denm-pki";

    /*
     * Configuration must enter through SimulationController.
     * VehicleSecurityInitializer remains the security consumer.
     */
    ns3::SimulationController::SetDenmPkiDirectory(pkiDir);

    ns3::VehicleSecurityInitializer::Initialize(state);

    const std::string expectedCert =
        pkiDir + "/veh0-ticket-000.cert";

    const std::string expectedKey =
        pkiDir + "/veh0-ticket-000.pem";

    Expect(
        state.security.certFile == expectedCert,
        "controller-provided PKI directory selects veh0 credential 000 certificate");

    Expect(
        state.security.keyFile == expectedKey,
        "controller-provided PKI directory selects veh0 credential 000 private key");

    Expect(
        state.security.denmCredentialPool.Size() >= 1,
        "controller-provided PKI directory discovers at least one DENM credential");

    Expect(
        state.security.denmCredentialPool.Empty() ||
            (state.security.denmCredentialPool.GetCredential(0) != nullptr &&
             state.security.denmCredentialPool.GetCredential(0)->credentialId ==
                "veh0-ticket-000"),
        "selected credential has explicit credential identifier");

    Expect(
        state.security.denmCredentialPool.Empty() ||
            (state.security.denmCredentialPool.GetCredential(0) != nullptr &&
             state.security.denmCredentialPool.GetCredential(0)->vehicleId == "veh0"),
        "selected credential is explicitly bound to veh0");

    Expect(
        state.security.denmCredentialPool.Empty() ||
            (state.security.denmCredentialPool.GetCredential(0) != nullptr &&
             state.security.denmCredentialPool.GetCredential(0)->sequence == 0),
        "selected credential carries sequence 000");

    Expect(
        std::filesystem::exists(expectedCert),
        "veh0 certificate exists");

    Expect(
        std::filesystem::exists(expectedKey),
        "veh0 private key exists");

    std::cout
        << "VEHICLE_SECURITY_PK_CONFIG_CONTROLLER_EXIT="
        << failures
        << '\n';

    return failures;
}
