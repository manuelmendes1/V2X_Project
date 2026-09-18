#include "vehicle_security_context.h"

namespace ns3
{

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
