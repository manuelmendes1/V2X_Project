#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"

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
        << "===== DENM MULTI-VEHICLE CREDENTIAL DISCOVERY TEST =====\n";

    constexpr const char* pkiDir =
        "/home/mmendes/vanetza-pki-lab/validation-denm-pki";

    const std::vector<std::string> vehicles = {
        "veh0",
        "veh1",
        "veh2",
        "veh3"
    };

    bool ok = true;

    for (const auto& vehId : vehicles)
    {
        const auto credentials =
            ns3::VehicleSecurityInitializer::DiscoverDenmCredentials(
                pkiDir,
                vehId);

        ok &= Check(
            credentials.size() >= 2,
            vehId + " has at least two discovered DENM credentials");

        if (!credentials.empty())
        {
            ok &= Check(
                credentials[0].certFile.find(
                    vehId + "-ticket") != std::string::npos,
                vehId + " credential certificate belongs to the vehicle");

            ok &= Check(
                credentials[0].keyFile.find(
                    vehId + "-ticket") != std::string::npos,
                vehId + " credential private key belongs to the vehicle");
        }
    }

    std::cout
        << "DENM_MULTI_VEHICLE_CREDENTIAL_DISCOVERY_EXIT="
        << (ok ? 0 : 1)
        << '\n';

    return ok ? 0 : 1;
}
