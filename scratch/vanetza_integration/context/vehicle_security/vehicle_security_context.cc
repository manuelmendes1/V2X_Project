#include "vehicle_security_context.h"

namespace ns3
{

bool
VehicleSecurityContext::SetActiveDenmCredential(
    std::size_t index)
{
    if (index >= denmCredentials.size())
    {
        return false;
    }

    const auto& credential =
        denmCredentials[index];

    activeDenmCredentialIndex = index;

    certFile = credential.certFile;
    keyFile = credential.keyFile;

    return true;
}

bool
VehicleSecurityContext::RotateToNextDenmCredential()
{
    if (denmCredentials.empty())
    {
        return false;
    }

    if (activeDenmCredentialIndex + 1 >= denmCredentials.size())
    {
        return false;
    }

    return SetActiveDenmCredential(
        activeDenmCredentialIndex + 1);
}

VehicleSecurityContext::~VehicleSecurityContext()
{
    if (privateKey != nullptr)
    {
        EVP_PKEY_free(privateKey);
        privateKey = nullptr;
    }

    if (certificate != nullptr)
    {
        X509_free(certificate);
        certificate = nullptr;
    }
}

} // namespace ns3
