#pragma once

#include <vanetza/security/private_key.hpp>
#include <vanetza/security/v3/certificate.hpp>

namespace ns3
{

struct DenmV3CredentialMaterial
{
    vanetza::security::v3::Certificate certificate;
    vanetza::security::PrivateKey privateKey;
};

} // namespace ns3
