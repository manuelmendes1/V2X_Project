#include "cpm_v3_sign_service.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential_binding_validator.h"

#include <filesystem>
#include <stdexcept>
#include <utility>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/security/backend.hpp>
#include <vanetza/security/persistence.hpp>
#include <vanetza/security/v3/certificate_validator.hpp>
#include <vanetza/security/v3/issuer_memory_lookup.hpp>
#include <vanetza/security/v3/location_checker.hpp>
#include <vanetza/security/v3/persistence.hpp>
#include <vanetza/security/v3/sign_header_policy.hpp>
#include <vanetza/security/v3/sign_service.hpp>
#include <vanetza/security/v3/static_certificate_provider.hpp>
#include <vanetza/security/v3/trust_store.hpp>

namespace ns3
{

namespace
{
constexpr vanetza::ItsAid kCpmItsAid = 639;

std::filesystem::path
RequirePkiFile(
    const std::filesystem::path& directory,
    const char* filename)
{
    const auto path = directory / filename;
    std::error_code ec;

    if (!std::filesystem::is_regular_file(path, ec) || ec)
    {
        throw std::runtime_error(
            "CPM PKI configuration is incomplete");
    }

    return path;
}
} // namespace

struct CpmV3SignService::Impl
{
    vanetza::ManualRuntime& runtime;
    vanetza::PositionProvider& position;

    std::unique_ptr<vanetza::security::Backend> backend;
    vanetza::security::v3::TrustStore trustStore;
    vanetza::security::v3::IssuerMemoryLookup issuerLookup;
    vanetza::security::v3::DefaultLocationChecker locationChecker;
    vanetza::security::v3::DefaultCertificateValidator certificateValidator;

    std::unique_ptr<
        vanetza::security::v3::StaticCertificateProvider>
        certificateProvider;

    std::unique_ptr<
        vanetza::security::v3::DefaultSignHeaderPolicy>
        signHeaderPolicy;

    std::unique_ptr<
        vanetza::security::v3::StraightSignService>
        signer;

    Impl(
        vanetza::ManualRuntime& rt,
        vanetza::PositionProvider& pos,
        const std::string& pkiDirectory)
        : runtime(rt),
          position(pos)
    {
        if (pkiDirectory.empty())
        {
            throw std::runtime_error(
                "CPM PKI directory is required");
        }

        const std::filesystem::path directory(pkiDirectory);

        const auto rootPath =
            RequirePkiFile(directory, "cpm-root.cert");
        const auto aaPath =
            RequirePkiFile(directory, "cpm-aa.cert");
        const auto ticketPath =
            RequirePkiFile(directory, "cpm-rsu-ticket-000.cert");
        const auto keyPath =
            RequirePkiFile(directory, "cpm-rsu-ticket-000.pem");

        const auto root =
            vanetza::security::v3::load_certificate_from_file(
                rootPath.string());
        const auto aa =
            vanetza::security::v3::load_certificate_from_file(
                aaPath.string());
        const auto ticket =
            vanetza::security::v3::load_certificate_from_file(
                ticketPath.string());
        const auto privateKey =
            vanetza::security::load_private_key_from_pem_file(
                keyPath.string());

        // Reject an incorrectly paired ticket/key before the signer is
        // initialized. Reuse the existing credential binding validator.
        VehicleCredential signingCredential{};
        signingCredential.certFile = ticketPath.string();
        signingCredential.keyFile = keyPath.string();

        if (!VehicleCredentialBindingValidator::Validate(signingCredential))
        {
            throw std::runtime_error(
                "CPM ticket certificate/private key binding validation failed");
        }

        if (!issuerLookup.insert(aa) ||
            !issuerLookup.insert(root))
        {
            throw std::runtime_error(
                "CPM PKI issuer configuration is invalid");
        }

        // The root alone is explicitly trusted; the AA is an issuer,
        // not an independent trust anchor.
        trustStore.insert(root);

        backend =
            vanetza::security::create_backend("default");

        if (!backend)
        {
            throw std::runtime_error(
                "Unable to create CPM security backend");
        }

        certificateValidator.use_runtime(&runtime);
        certificateValidator.use_position_provider(&position);
        certificateValidator.use_location_checker(&locationChecker);
        certificateValidator.use_issuer_lookup(&issuerLookup);
        certificateValidator.use_trust_store(&trustStore);

        // Keep the certificate, key and policy alive for the lifetime
        // of StraightSignService.
        certificateProvider =
            std::make_unique<
                vanetza::security::v3::StaticCertificateProvider>(
                    ticket,
                    privateKey);

        signHeaderPolicy =
            std::make_unique<
                vanetza::security::v3::DefaultSignHeaderPolicy>(
                    runtime,
                    position,
                    *certificateProvider);

        // Request the full ticket certificate in the initial transmission.
        // Subsequent certificate distribution/cache behavior is a separate
        // interoperability concern and must be validated independently.
        signHeaderPolicy->request_certificate();

        signer =
            std::make_unique<
                vanetza::security::v3::StraightSignService>(
                    *certificateProvider,
                    *backend,
                    *signHeaderPolicy,
                    certificateValidator);
    }
};

CpmV3SignService::CpmV3SignService(
    vanetza::ManualRuntime& runtime,
    vanetza::PositionProvider& position,
    const std::string& pkiDirectory)
    : m_impl(std::make_unique<Impl>(
          runtime, position, pkiDirectory))
{
}

CpmV3SignService::~CpmV3SignService() = default;

vanetza::security::SignConfirm
CpmV3SignService::sign(
    vanetza::security::SignRequest&& request)
{
    if (request.its_aid != kCpmItsAid)
    {
        return vanetza::security::SignConfirm::failure(
            vanetza::security::SignConfirmError::No_Certificate);
    }

    // StraightSignService validates the configured chain and the ticket's
    // permission for AID 639 before producing the signed message.
    return m_impl->signer->sign(std::move(request));
}

} // namespace ns3
