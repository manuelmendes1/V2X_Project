#include "vanetza_integration/context/vehicle_security/active_credential_provider.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential_pool.h"

#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

namespace
{

VehicleCredential MakeCredential(
    const std::string& id,
    const std::string& vehicle,
    std::size_t sequence)
{
    return VehicleCredential{
        id,
        vehicle,
        sequence,
        id + ".cert",
        id + ".pem"};
}

int Fail(const std::string& message)
{
    std::cerr << "FAIL " << message << std::endl;
    return 1;
}

int Pass(const std::string& message)
{
    std::cout << "PASS " << message << std::endl;
    return 0;
}

} // namespace

int main()
{
    VehicleCredentialPool pool;

    const ActiveCredentialProvider& provider = pool;

    if (provider.GetActiveCredential() != nullptr)
    {
        return Fail("empty pool has no active credential");
    }
    Pass("empty pool exposes no active credential");

    pool.SetCredentials({
        MakeCredential("veh0-ticket-000", "veh0", 0),
        MakeCredential("veh0-ticket-001", "veh0", 1),
        MakeCredential("veh0-ticket-002", "veh0", 2)});

    const VehicleCredential* active = provider.GetActiveCredential();

    if (active == nullptr)
    {
        return Fail("populated pool exposes an active credential");
    }
    Pass("populated pool exposes an active credential");

    if (active->credentialId != "veh0-ticket-000")
    {
        return Fail("provider exposes credential[0] as initial active credential");
    }
    Pass("provider exposes credential[0] as initial active credential");

    if (!pool.Select(2))
    {
        return Fail("pool selects credential[2]");
    }

    active = provider.GetActiveCredential();

    if (active == nullptr)
    {
        return Fail("provider remains valid after pool selection");
    }

    if (active->credentialId != "veh0-ticket-002")
    {
        return Fail("provider follows pool active selection");
    }
    Pass("provider follows pool active selection");

    std::cout << "ACTIVE_CREDENTIAL_PROVIDER_TEST_EXIT=0" << std::endl;
    return 0;
}
