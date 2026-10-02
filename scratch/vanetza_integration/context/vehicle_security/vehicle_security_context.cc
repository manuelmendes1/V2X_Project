#include "vehicle_security_context.h"

#include "vanetza_integration/context/vehicle_security/vehicle_credential_binding_validator.h"
#include "vanetza_integration/security/vehicle_crypto/vehicle_crypto.h"

namespace ns3
{

bool
VehicleSecurityContext::SetActiveDenmCredential(
    std::size_t index)
{
    const VehicleCredential* credential =
        credentialPool.GetCredential(index);

    if (credential == nullptr ||
        credential->vehicleId.empty() ||
        credential->keyFile.empty() ||
        credential->certFile.empty())
    {
        return false;
    }

    /*
     * A credential may become active only after its ETSI V3 certificate
     * and private key have been proven to belong together. Perform this
     * validation before staging CAM material or mutating pool selection.
     */
    if (!VehicleCredentialBindingValidator::Validate(*credential))
    {
        return false;
    }

    /*
     * Stage the CAM/OpenSSL representation before mutating the current
     * security state. LoadEcdsaKeyAndCert() consumes the credential's
     * private-key material and creates the OpenSSL certificate used by CAM.
     */
    auto [newPrivateKey, newCertificate] =
        LoadEcdsaKeyAndCert(
            credential->keyFile,
            credential->vehicleId);

    if (newPrivateKey == nullptr ||
        newCertificate == nullptr)
    {
        if (newPrivateKey != nullptr)
        {
            EVP_PKEY_free(newPrivateKey);
        }

        if (newCertificate != nullptr)
        {
            X509_free(newCertificate);
        }

        return false;
    }

    /*
     * The index is known to be valid, so this should succeed. Do not commit
     * any other state until selection has succeeded.
     */
    if (!credentialPool.Select(index))
    {
        EVP_PKEY_free(newPrivateKey);
        X509_free(newCertificate);
        return false;
    }

    EVP_PKEY* oldPrivateKey =
        privateKey;

    X509* oldCertificate =
        certificate;

    certFile =
        credential->certFile;

    keyFile =
        credential->keyFile;

    privateKey =
        newPrivateKey;

    certificate =
        newCertificate;

    if (oldPrivateKey != nullptr)
    {
        EVP_PKEY_free(oldPrivateKey);
    }

    if (oldCertificate != nullptr)
    {
        X509_free(oldCertificate);
    }

    return true;
}

bool
VehicleSecurityContext::RotateToNextDenmCredential()
{
    if (credentialPool.Empty())
    {
        return false;
    }

    const std::size_t currentIndex =
        credentialPool.GetActiveCredentialIndex();

    if (currentIndex + 1 >= credentialPool.Size())
    {
        return false;
    }

    return SetActiveDenmCredential(
        currentIndex + 1);
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
