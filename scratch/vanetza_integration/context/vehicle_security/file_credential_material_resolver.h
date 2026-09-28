#pragma once

#include "vanetza_integration/context/vehicle_security/credential_material_resolver.h"

namespace ns3
{

class FileCredentialMaterialResolver final
    : public CredentialMaterialResolver
{
public:
    std::unique_ptr<DenmV3CredentialMaterial>
    Resolve(const VehicleCredential& credential) const override;
};

} // namespace ns3
