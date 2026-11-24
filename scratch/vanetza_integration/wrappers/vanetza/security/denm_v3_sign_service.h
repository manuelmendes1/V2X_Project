#pragma once

#include <memory>
#include <string>

#include "vanetza_integration/context/vehicle_security/active_credential_provider.h"
#include "vanetza_integration/context/vehicle_security/credential_material_resolver.h"

#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_provider.hpp>

#include <vanetza/security/backend.hpp>
#include <vanetza/security/sign_service.hpp>

#include <vanetza/security/v3/certificate_provider.hpp>
#include <vanetza/security/v3/certificate_validator.hpp>
#include <vanetza/security/v3/issuer_memory_lookup.hpp>
#include <vanetza/security/v3/location_checker.hpp>
#include <vanetza/security/v3/sign_header_policy.hpp>
#include <vanetza/security/v3/sign_service.hpp>
#include <vanetza/security/v3/static_certificate_provider.hpp>
#include <vanetza/security/v3/trust_store.hpp>

namespace ns3
{

class DenmV3SignService final : public vanetza::security::SignService
{
public:
    DenmV3SignService(
        vanetza::ManualRuntime& runtime,
        vanetza::PositionProvider& position,
        const ActiveCredentialProvider& credentialProvider,
        CredentialMaterialResolver& materialResolver,
        const std::string& vehiclePkiRunDirectory);

    bool ReloadActiveCredential();

    vanetza::security::SignConfirm sign(
        vanetza::security::SignRequest&& request) override;

private:
    std::unique_ptr<vanetza::security::Backend> m_backend;

    // The validator stores non-owning pointers to these trust-domain objects.
    // Their declaration order keeps them alive for the validator's lifetime.
    vanetza::security::v3::TrustStore m_trustStore;
    vanetza::security::v3::IssuerMemoryLookup m_issuerLookup;
    vanetza::security::v3::DefaultLocationChecker m_locationChecker;

    vanetza::security::v3::DefaultCertificateValidator
        m_certificateValidator;

    std::unique_ptr<
        vanetza::security::v3::StaticCertificateProvider>
        m_certificateProvider;

    std::unique_ptr<
        vanetza::security::v3::DefaultSignHeaderPolicy>
        m_signHeaderPolicy;

    // Shared ETSI V3 signer for the vehicle's CAM and DENM tickets.
    std::unique_ptr<
        vanetza::security::v3::StraightSignService>
        m_denmSigner;

    const ActiveCredentialProvider& m_credentialProvider;
    CredentialMaterialResolver& m_materialResolver;

    vanetza::ManualRuntime& m_runtime;
    vanetza::PositionProvider& m_position;
};

} // namespace ns3
