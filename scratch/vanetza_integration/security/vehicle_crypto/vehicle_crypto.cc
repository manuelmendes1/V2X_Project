#include "vehicle_crypto.h"

#include <cstdlib>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace ns3 {

std::pair<EVP_PKEY*, X509*> GenerateEcdsaKeyAndCert(const std::string& vehId){

    EVP_PKEY* pkey = EVP_PKEY_new();

    EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

    EC_KEY_generate_key(eckey);

    EVP_PKEY_assign_EC_KEY(pkey, eckey);

    X509* cert = X509_new();

    ASN1_INTEGER_set(X509_get_serialNumber(cert),std::rand());

    X509_gmtime_adj(X509_get_notBefore(cert),0);

    X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);

    X509_set_pubkey(cert, pkey);

    X509_NAME* name = X509_get_subject_name(cert);

    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)vehId.c_str(), -1, -1, 0);

    X509_set_issuer_name(cert, name);

    X509_sign(cert, pkey, EVP_sha256());

    return {pkey, cert};
}

}

#pragma GCC diagnostic pop
