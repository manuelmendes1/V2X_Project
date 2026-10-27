#include "file_credential_material_resolver.h"
#include <vanetza/security/persistence.hpp>

#include "vanetza_integration/context/vehicle_security/vehicle_credential_binding_validator.h"

#include <vanetza/security/v3/persistence.hpp>

#include <memory>
#include <utility>

namespace ns3
{

namespace
{

vanetza::security::PrivateKey
LoadPrivateKey(const std::string& path)
{
    return vanetza::security::load_private_key_from_pem_file(path);
}

} // namespace

std::unique_ptr<DenmV3CredentialMaterial>
FileCredentialMaterialResolver::Resolve(
    const VehicleCredential& credential) const
{
    if (!VehicleCredentialBindingValidator::Validate(credential))
    {
        return nullptr;
    }

    try
    {
        auto material =
            std::make_unique<DenmV3CredentialMaterial>();

        material->certificate =
            vanetza::security::v3::load_certificate_from_file(
                credential.certFile);

        material->privateKey =
            LoadPrivateKey(credential.keyFile);

        return material;
    }
    catch (...)
    {
        return nullptr;
    }
}

} // namespace ns3
