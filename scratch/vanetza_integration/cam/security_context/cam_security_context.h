#pragma once

#include <openssl/evp.h>
#include <openssl/x509.h>

namespace ns3
{

struct CamSecurityContext
{
    EVP_PKEY* privateKey = nullptr;

    X509* certificate = nullptr;
};

} // namespace ns3
