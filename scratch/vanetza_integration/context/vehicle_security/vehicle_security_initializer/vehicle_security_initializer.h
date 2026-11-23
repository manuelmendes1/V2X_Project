#pragma once

#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"
#include "vanetza_integration/context/vehicle_state.h"

#include <string>
#include <vector>

namespace ns3
{

struct VehicleCredentialMapping
{
    std::string sumoVehicleId;
    std::string credentialDirectoryRelative;
    std::string credentialFilePrefix;
};

class VehicleSecurityInitializer
{
public:
    static void SetDenmPkiDirectory(const std::string& directory);

    // Configures strict vehicle PKI mode. Both paths must be provided
    // together. Passing both empty disables explicit vehicle PKI mode.
    static bool ConfigureVehiclePki(
        const std::string& runDirectory,
        const std::string& mappingFile,
        std::string& error);

    // Loads the SUMO-ID to credential-directory/prefix mapping.
    // The output is empty on failure; error contains the diagnostic.
    static bool LoadVehicleCredentialMappings(
        const std::string& mappingFile,
        std::vector<VehicleCredentialMapping>& mappings,
        std::string& error);

    // Resolves a mapping directory and rejects paths escaping the PKI run.
    static bool ResolveVehicleCredentialDirectory(
        const std::string& pkiRunDirectory,
        const VehicleCredentialMapping& mapping,
        std::string& resolvedDirectory,
        std::string& error);

    // Generic ticket discovery. SUMO identity and credential filename
    // prefix are deliberately independent values.
    static std::vector<VehicleCredential>
    DiscoverVehicleCredentials(
        const std::string& directory,
        const std::string& sumoVehicleId,
        const std::string& credentialFilePrefix);

    // Backward-compatible discovery convention.
    static std::vector<VehicleCredential>
    DiscoverDenmCredentials(
        const std::string& directory,
        const std::string& vehId);

    // Initializes vehicle security. Explicit PKI mode is fail-closed.
    static bool Initialize(
        VehicleState& state,
        std::string& error);
};

} // namespace ns3
