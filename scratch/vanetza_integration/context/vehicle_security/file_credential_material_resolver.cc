#include "file_credential_material_resolver.h"

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
    const auto keyPair =
        vanetza::security::v3::load_private_key_from_file(path);

    vanetza::security::PrivateKey key;
    key.type = vanetza::security::KeyType::NistP256;
    key.key.assign(
        keyPair.private_key.key.begin(),
        keyPair.private_key.key.end());

    return key;
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
