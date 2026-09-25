#pragma once

#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"
#include "vanetza_integration/context/vehicle_state.h"

#include <string>
#include <vector>

namespace ns3
{

class VehicleSecurityInitializer
{
public:
    static void SetDenmPkiDirectory(const std::string& directory);

    static std::vector<VehicleCredential>
    DiscoverDenmCredentials(
        const std::string& directory,
        const std::string& vehId);

    static void Initialize(VehicleState& state);
};

} // namespace ns3
