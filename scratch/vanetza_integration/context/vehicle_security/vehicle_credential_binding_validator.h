#pragma once

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

            const auto keyPair =
                vanetza::security::v3::load_private_key_from_file(credential.keyFile);

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

            if (certificatePublicKey->x.size() != keyPair.public_key.x.size())
            {
                return false;
            }

            const bool xMatch =
                std::equal(certificatePublicKey->x.begin(),
                           certificatePublicKey->x.end(),
                           keyPair.public_key.x.begin());

            if (!xMatch)
            {
                return false;
            }

            switch (certificatePublicKey->compression)
            {
                case vanetza::security::KeyCompression::NoCompression:
                {
                    if (certificatePublicKey->y.size() != keyPair.public_key.y.size())
                    {
                        return false;
                    }

                    return std::equal(certificatePublicKey->y.begin(),
                                      certificatePublicKey->y.end(),
                                      keyPair.public_key.y.begin());
                }

                case vanetza::security::KeyCompression::Y0:
                {
                    if (keyPair.public_key.y.empty())
                    {
                        return false;
                    }

                    return (keyPair.public_key.y.back() & 0x01U) == 0U;
                }

                case vanetza::security::KeyCompression::Y1:
                {
                    if (keyPair.public_key.y.empty())
                    {
                        return false;
                    }

                    return (keyPair.public_key.y.back() & 0x01U) == 1U;
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
