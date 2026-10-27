#pragma once

#include <vanetza/security/persistence.hpp>
#include <vanetza/security/backend_openssl.hpp>

#include "vehicle_credential.h"

#include <algorithm>

#include <vanetza/security/key_type.hpp>
#include <vanetza/security/v3/certificate.hpp>
#include <vanetza/security/v3/persistence.hpp>

namespace ns3
{

class VehicleCredentialBindingValidator
{
public:
    static bool Validate(const VehicleCredential& credential)
    {
        if (credential.certFile.empty() || credential.keyFile.empty())
        {
            return false;
        }

        try
        {
            const auto certificate =
                vanetza::security::v3::load_certificate_from_file(credential.certFile);

            const auto privateKey =
                vanetza::security::load_private_key_from_pem_file(credential.keyFile);

            if (privateKey.type != vanetza::security::KeyType::NistP256)
            {
                return false;
            }

            // Derive the public point from the loaded private scalar using
            // the configured OpenSSL backend, then compare it with the
            // public key carried by the certificate.
            const auto derivedPublicKey =
                vanetza::security::openssl::derive_public_key(privateKey);

            if (derivedPublicKey.type != vanetza::security::KeyType::NistP256)
            {
                return false;
            }

            const auto certificatePublicKey =
                vanetza::security::v3::get_public_key(*certificate.content());

            if (!certificatePublicKey)
            {
                return false;
            }

            if (certificatePublicKey->type != vanetza::security::KeyType::NistP256)
            {
                return false;
            }

            if (certificatePublicKey->x.size() != derivedPublicKey.x.size())
            {
                return false;
            }

            const bool xMatch =
                std::equal(certificatePublicKey->x.begin(),
                           certificatePublicKey->x.end(),
                           derivedPublicKey.x.begin());

            if (!xMatch)
            {
                return false;
            }

            switch (certificatePublicKey->compression)
            {
                case vanetza::security::KeyCompression::NoCompression:
                {
                    if (certificatePublicKey->y.size() != derivedPublicKey.y.size())
                    {
                        return false;
                    }

                    return std::equal(certificatePublicKey->y.begin(),
                                      certificatePublicKey->y.end(),
                                      derivedPublicKey.y.begin());
                }

                case vanetza::security::KeyCompression::Y0:
                {
                    if (derivedPublicKey.y.empty())
                    {
                        return false;
                    }

                    return (derivedPublicKey.y.back() & 0x01U) == 0U;
                }

                case vanetza::security::KeyCompression::Y1:
                {
                    if (derivedPublicKey.y.empty())
                    {
                        return false;
                    }

                    return (derivedPublicKey.y.back() & 0x01U) == 1U;
                }
            }

            return false;
        }
        catch (...)
        {
            return false;
        }
    }
};

} // namespace ns3
