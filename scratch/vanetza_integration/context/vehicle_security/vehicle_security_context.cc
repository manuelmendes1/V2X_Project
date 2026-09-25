#include "vehicle_security_context.h"

namespace ns3
{

bool
VehicleSecurityContext::SetActiveDenmCredential(
    std::size_t index)
{
    if (!denmCredentialPool.Select(index))
    {
        return false;
    }

    const auto* credential =
        denmCredentialPool.GetActiveCredential();

    if (credential == nullptr)
    {
        return false;
    }

    certFile = credential->certFile;
    keyFile = credential->keyFile;

    return true;
}

bool
VehicleSecurityContext::RotateToNextDenmCredential()
{
    if (!denmCredentialPool.SelectNext())
    {
        return false;
    }

    const auto* credential =
        denmCredentialPool.GetActiveCredential();

    if (credential == nullptr)
    {
        return false;
    }

    certFile = credential->certFile;
    keyFile = credential->keyFile;

    return true;
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
