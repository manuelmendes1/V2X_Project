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
        << "===== DENM CREDENTIAL DISCOVERY TEST =====\n";

    const fs::path root =
        fs::temp_directory_path() / "denm_credential_discovery_test";

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

    // veh0 sequence
    CreateFile(root / "veh0-ticket-002.cert");
    CreateFile(root / "veh0-ticket-002.pem");

    CreateFile(root / "veh0-ticket-000.cert");
    CreateFile(root / "veh0-ticket-000.pem");

    CreateFile(root / "veh0-ticket-001.cert");
    CreateFile(root / "veh0-ticket-001.pem");

    // Different vehicle: MUST NOT be discovered for veh0.
    CreateFile(root / "veh1-ticket-000.cert");
    CreateFile(root / "veh1-ticket-000.pem");

    const auto credentials =
        ns3::VehicleSecurityInitializer::DiscoverDenmCredentials(
            root.string(),
            "veh0");

    bool ok = true;

    ok &= Check(
        credentials.size() == 3,
        "veh0 discovers exactly three sequential credentials");

    if (credentials.size() == 3)
    {
        ok &= Check(
            credentials[0].certFile.ends_with("veh0-ticket-000.cert"),
            "credential sequence starts at 000");

        ok &= Check(
            credentials[1].certFile.ends_with("veh0-ticket-001.cert"),
            "credential sequence contains 001");

        ok &= Check(
            credentials[2].certFile.ends_with("veh0-ticket-002.cert"),
            "credential sequence contains 002");

        ok &= Check(
            credentials[0].keyFile.ends_with("veh0-ticket-000.pem"),
            "credential 000 uses matching private key");

        ok &= Check(
            credentials[1].keyFile.ends_with("veh0-ticket-001.pem"),
            "credential 001 uses matching private key");

        ok &= Check(
            credentials[2].keyFile.ends_with("veh0-ticket-002.pem"),
            "credential 002 uses matching private key");

        ok &= Check(
            credentials[0].credentialId == "veh0-ticket-000",
            "credential 000 has explicit logical identifier");

        ok &= Check(
            credentials[1].credentialId == "veh0-ticket-001",
            "credential 001 has explicit logical identifier");

        ok &= Check(
            credentials[2].credentialId == "veh0-ticket-002",
            "credential 002 has explicit logical identifier");

        ok &= Check(
            credentials[0].vehicleId == "veh0",
            "credential 000 is explicitly bound to veh0");

        ok &= Check(
            credentials[1].vehicleId == "veh0",
            "credential 001 is explicitly bound to veh0");

        ok &= Check(
            credentials[2].vehicleId == "veh0",
            "credential 002 is explicitly bound to veh0");

        ok &= Check(
            credentials[0].sequence == 0,
            "credential 000 carries sequence 0");

        ok &= Check(
            credentials[1].sequence == 1,
            "credential 001 carries sequence 1");

        ok &= Check(
            credentials[2].sequence == 2,
            "credential 002 carries sequence 2");
    }


    // Integration contract:
    // VehicleSecurityContext must retain the discovered pool and expose
    // credential 000 as the initial active credential.
    ns3::VehicleState state;
    state.identity.vehId = "veh0";

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        root.string());

    ns3::VehicleSecurityInitializer::Initialize(state);

    ok &= Check(
        state.security.credentialPool.Size() == 3,
        "VehicleSecurityContext stores the three discovered credentials");

    ok &= Check(
        state.security.credentialPool.GetActiveCredentialIndex() == 0,
        "initial active DENM credential is sequence 000");

    if (state.security.credentialPool.Size() == 3)
    {
        ok &= Check(
            state.security.credentialPool.GetCredential(0)->certFile.ends_with(
                "veh0-ticket-000.cert"),
            "context credential 000 certificate is correct");

        ok &= Check(
            state.security.credentialPool.GetCredential(0)->keyFile.ends_with(
                "veh0-ticket-000.pem"),
            "context credential 000 private key is correct");

        ok &= Check(
            state.security.certFile ==
                state.security.credentialPool.GetCredential(0)->certFile,
            "active certFile matches selected credential");

        ok &= Check(
            state.security.keyFile ==
                state.security.credentialPool.GetCredential(0)->keyFile,
            "active keyFile matches selected credential");
    }


    // ------------------------------------------------------------------
    // Active credential selection contract.
    // ------------------------------------------------------------------
    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        root.string());

    ns3::VehicleState selectionState;
    selectionState.identity.vehId = "veh0";

    ns3::VehicleSecurityInitializer::Initialize(
        selectionState);

    ok &= Check(
        selectionState.security.SetActiveDenmCredential(1),
        "second DENM credential can be selected");

    ok &= Check(
        selectionState.security.credentialPool.GetActiveCredentialIndex() == 1,
        "active DENM credential index changes to sequence 001");

    ok &= Check(
        selectionState.security.certFile.ends_with(
            "veh0-ticket-001.cert"),
        "active certFile changes to credential 001");

    ok &= Check(
        selectionState.security.keyFile.ends_with(
            "veh0-ticket-001.pem"),
        "active keyFile changes to credential 001");

    ok &= Check(
        !selectionState.security.SetActiveDenmCredential(99),
        "out-of-range DENM credential selection is rejected");

    ok &= Check(
        selectionState.security.credentialPool.GetActiveCredentialIndex() == 1,
        "rejected selection preserves the active credential");

    ok &= Check(
        selectionState.security.certFile.ends_with(
            "veh0-ticket-001.cert"),
        "rejected selection preserves active certificate");

    ok &= Check(
        selectionState.security.keyFile.ends_with(
            "veh0-ticket-001.pem"),
        "rejected selection preserves active private key");

    // Clear process-wide test configuration before continuing.
    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory("");

    // Legacy compatibility contract.
    const fs::path legacyRoot =
        root / "legacy";

    fs::create_directories(legacyRoot, ec);

    CreateFile(legacyRoot / "veh0-ticket.cert");
    CreateFile(legacyRoot / "veh0-ticket.pem");

    const auto legacyCredentials =
        ns3::VehicleSecurityInitializer::DiscoverDenmCredentials(
            legacyRoot.string(),
            "veh0");

    ok &= Check(
        legacyCredentials.size() == 1,
        "legacy veh0 ticket layout remains discoverable");

    if (legacyCredentials.size() == 1)
    {
        ok &= Check(
            legacyCredentials[0].certFile.ends_with("veh0-ticket.cert"),
            "legacy certificate path is preserved");

        ok &= Check(
            legacyCredentials[0].keyFile.ends_with("veh0-ticket.pem"),
            "legacy private key path is preserved");
    }

    fs::remove_all(root, ec);

    std::cout
        << "DENM_CREDENTIAL_DISCOVERY_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
