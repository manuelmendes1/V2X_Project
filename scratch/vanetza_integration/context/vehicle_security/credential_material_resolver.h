#pragma once

#include <memory>

#include "vanetza_integration/context/vehicle_security/denm_credential_material.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"

namespace ns3
{

class CredentialMaterialResolver
{
public:
    virtual ~CredentialMaterialResolver() = default;

    virtual std::unique_ptr<DenmV3CredentialMaterial>
    Resolve(const VehicleCredential& credential) const = 0;
};

} // namespace ns3
