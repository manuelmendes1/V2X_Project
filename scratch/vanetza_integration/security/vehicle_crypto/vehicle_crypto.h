#pragma once

#include <string>
#include <utility>

#include <openssl/evp.h>
#include <openssl/x509.h>

namespace ns3 {

std::pair<EVP_PKEY*, X509*> GenerateEcdsaKeyAndCert(const std::string& vehId);

}
