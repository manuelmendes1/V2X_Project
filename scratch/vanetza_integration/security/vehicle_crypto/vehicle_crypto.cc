#include "vehicle_crypto.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace ns3
{

namespace
{

X509*
CreateEcdsaCertificate(
    EVP_PKEY* pkey,
    const std::string& vehId)
{
    if (pkey == nullptr)
    {
        return nullptr;
    }

    X509* cert = X509_new();

    if (cert == nullptr)
    {
        return nullptr;
    }

    if (ASN1_INTEGER_set(
            X509_get_serialNumber(cert),
            std::rand()) != 1)
    {
        X509_free(cert);
        return nullptr;
    }

    if (X509_gmtime_adj(
            X509_get_notBefore(cert),
            0) == nullptr)
    {
        X509_free(cert);
        return nullptr;
    }

    if (X509_gmtime_adj(
            X509_get_notAfter(cert),
            365 * 24 * 3600) == nullptr)
    {
        X509_free(cert);
        return nullptr;
    }

    if (X509_set_pubkey(cert, pkey) != 1)
    {
        X509_free(cert);
        return nullptr;
    }

    X509_NAME* name =
        X509_get_subject_name(cert);

    if (name == nullptr)
    {
        X509_free(cert);
        return nullptr;
    }

    if (X509_NAME_add_entry_by_txt(
            name,
            "CN",
            MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>(
                vehId.c_str()),
            -1,
            -1,
            0) != 1)
    {
        X509_free(cert);
        return nullptr;
    }

    if (X509_set_issuer_name(cert, name) != 1)
    {
        X509_free(cert);
        return nullptr;
    }

    if (X509_sign(
            cert,
            pkey,
            EVP_sha256()) == 0)
    {
        X509_free(cert);
        return nullptr;
    }

    return cert;
}

} // namespace

std::pair<EVP_PKEY*, X509*>
GenerateEcdsaKeyAndCert(
    const std::string& vehId)
{
    EVP_PKEY* pkey =
        EVP_PKEY_new();

    if (pkey == nullptr)
    {
        return {nullptr, nullptr};
    }

    EC_KEY* eckey =
        EC_KEY_new_by_curve_name(
            NID_X9_62_prime256v1);

    if (eckey == nullptr)
    {
        EVP_PKEY_free(pkey);
        return {nullptr, nullptr};
    }

    if (EC_KEY_generate_key(eckey) != 1)
    {
        EC_KEY_free(eckey);
        EVP_PKEY_free(pkey);
        return {nullptr, nullptr};
    }

    if (EVP_PKEY_assign_EC_KEY(
            pkey,
            eckey) != 1)
    {
        EC_KEY_free(eckey);
        EVP_PKEY_free(pkey);
        return {nullptr, nullptr};
    }

    X509* cert =
        CreateEcdsaCertificate(
            pkey,
            vehId);

    if (cert == nullptr)
    {
        EVP_PKEY_free(pkey);
        return {nullptr, nullptr};
    }

    return {pkey, cert};
}

std::pair<EVP_PKEY*, X509*>
LoadEcdsaKeyAndCert(
    const std::string& keyFile,
    const std::string& vehId)
{
    FILE* file =
        std::fopen(
            keyFile.c_str(),
            "rb");

    if (file == nullptr)
    {
        return {nullptr, nullptr};
    }

    EVP_PKEY* pkey =
        PEM_read_PrivateKey(
            file,
            nullptr,
            nullptr,
            nullptr);

    std::fclose(file);

    if (pkey == nullptr)
    {
        return {nullptr, nullptr};
    }

    if (EVP_PKEY_base_id(pkey) != EVP_PKEY_EC)
    {
        EVP_PKEY_free(pkey);
        return {nullptr, nullptr};
    }

    if (EVP_PKEY_get_bits(pkey) != 256)
    {
        EVP_PKEY_free(pkey);
        return {nullptr, nullptr};
    }

    X509* cert =
        CreateEcdsaCertificate(
            pkey,
            vehId);

    if (cert == nullptr)
    {
        EVP_PKEY_free(pkey);
        return {nullptr, nullptr};
    }

    return {pkey, cert};
}

} // namespace ns3

#pragma GCC diagnostic pop
