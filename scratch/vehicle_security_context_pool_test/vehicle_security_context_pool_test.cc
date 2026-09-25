#include "vanetza_integration/context/vehicle_security/vehicle_security_context.h"

#include <iostream>
#include <string>
#include <vector>

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
        << "===== VEHICLE SECURITY CONTEXT POOL TEST =====\n";

    const ns3::VehicleCredential credential000{
        "veh0-ticket-000",
        "veh0",
        0,
        "/tmp/veh0-ticket-000.cert",
        "/tmp/veh0-ticket-000.pem"};

    const ns3::VehicleCredential credential001{
        "veh0-ticket-001",
        "veh0",
        1,
        "/tmp/veh0-ticket-001.cert",
        "/tmp/veh0-ticket-001.pem"};

    const ns3::VehicleCredential credential002{
        "veh0-ticket-002",
        "veh0",
        2,
        "/tmp/veh0-ticket-002.cert",
        "/tmp/veh0-ticket-002.pem"};

    ns3::VehicleSecurityContext security;
    bool ok = true;

    ok &= Check(
        security.denmCredentialPool.Empty(),
        "security context starts with an empty DENM credential pool");

    security.denmCredentialPool.SetCredentials({
        credential000,
        credential001,
        credential002});

    ok &= Check(
        security.denmCredentialPool.Size() == 3,
        "security context owns three DENM credentials through the pool");

    ok &= Check(
        security.denmCredentialPool.GetActiveCredential() != nullptr &&
            security.denmCredentialPool.GetActiveCredential()->credentialId ==
                "veh0-ticket-000",
        "pool exposes credential 000 as the initial active credential");

    ok &= Check(
        security.SetActiveDenmCredential(1),
        "security context selects DENM credential 001");

    ok &= Check(
        security.denmCredentialPool.GetActiveCredentialIndex() == 1,
        "pool active index changes to credential 001");

    ok &= Check(
        security.certFile.ends_with("veh0-ticket-001.cert"),
        "security context certFile follows selected credential");

    ok &= Check(
        security.keyFile.ends_with("veh0-ticket-001.pem"),
        "security context keyFile follows selected credential");

    ok &= Check(
        security.RotateToNextDenmCredential(),
        "security context rotates from credential 001 to 002");

    ok &= Check(
        security.denmCredentialPool.GetActiveCredentialIndex() == 2,
        "pool active index changes to credential 002");

    ok &= Check(
        security.certFile.ends_with("veh0-ticket-002.cert"),
        "security context certFile follows rotation");

    ok &= Check(
        !security.RotateToNextDenmCredential(),
        "rotation beyond the pool is rejected");

    ok &= Check(
        security.denmCredentialPool.GetActiveCredentialIndex() == 2,
        "rejected rotation preserves pool active index");

    ok &= Check(
        security.certFile.ends_with("veh0-ticket-002.cert"),
        "rejected rotation preserves active certificate");

    std::cout
        << "VEHICLE_SECURITY_CONTEXT_POOL_EXIT="
        << (ok ? 0 : 1)
        << '\n';

    return ok ? 0 : 1;
}
