#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace
{

void
CreateFile(const fs::path& path)
{
    std::ofstream file(path);
    file << "test";
}

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
        << "===== DENM CREDENTIAL POOL ROTATION TEST =====\n";

    const fs::path root =
        fs::temp_directory_path() / "denm_credential_pool_rotation_test";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    if (ec)
    {
        std::cerr
            << "FAIL could not create temporary PKI directory: "
            << ec.message() << "\n";
        return 1;
    }

    for (const char* seq : {"000", "001", "002"})
    {
        const std::string sequence(seq);

        CreateFile(
            root / ("veh0-ticket-" + sequence + ".cert"));

        CreateFile(
            root / ("veh0-ticket-" + sequence + ".pem"));
    }

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        root.string());

    ns3::VehicleState state;
    state.identity.vehId = "veh0";

    ns3::VehicleSecurityInitializer::Initialize(state);

    bool ok = true;

    ok &= Check(
        state.security.denmCredentials.size() == 3,
        "three DENM credentials are available");

    ok &= Check(
        state.security.activeDenmCredentialIndex == 0,
        "credential 000 is active initially");

    ok &= Check(
        state.security.certFile.ends_with("veh0-ticket-000.cert"),
        "credential 000 certificate is initially active");

    ok &= Check(
        state.security.RotateToNextDenmCredential(),
        "rotation from credential 000 to 001 succeeds");

    ok &= Check(
        state.security.activeDenmCredentialIndex == 1,
        "credential 001 becomes active");

    ok &= Check(
        state.security.certFile.ends_with("veh0-ticket-001.cert"),
        "credential 001 certificate is active");

    ok &= Check(
        state.security.RotateToNextDenmCredential(),
        "rotation from credential 001 to 002 succeeds");

    ok &= Check(
        state.security.activeDenmCredentialIndex == 2,
        "credential 002 becomes active");

    ok &= Check(
        state.security.certFile.ends_with("veh0-ticket-002.cert"),
        "credential 002 certificate is active");

    ok &= Check(
        !state.security.RotateToNextDenmCredential(),
        "rotation beyond the credential pool is rejected");

    ok &= Check(
        state.security.activeDenmCredentialIndex == 2,
        "rejected rotation preserves the last active credential");

    ok &= Check(
        state.security.certFile.ends_with("veh0-ticket-002.cert"),
        "rejected rotation preserves the active certificate");

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory("");

    fs::remove_all(root, ec);

    std::cout
        << "DENM_CREDENTIAL_POOL_ROTATION_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
