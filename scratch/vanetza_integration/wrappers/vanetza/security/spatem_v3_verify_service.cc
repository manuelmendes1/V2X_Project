#include "spatem_v3_verify_service.h"

#include <filesystem>
#include <stdexcept>

#include <ns3/log.h>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/common/runtime.hpp>
#include <vanetza/security/backend.hpp>
#include <vanetza/security/straight_verify_service.hpp>
#include <vanetza/security/v3/certificate_cache.hpp>
#include <vanetza/security/v3/certificate_validator.hpp>
#include <vanetza/security/v3/issuer_memory_lookup.hpp>
#include <vanetza/security/v3/location_checker.hpp>
#include <vanetza/security/v3/persistence.hpp>
#include <vanetza/security/v3/trust_store.hpp>

#include "vanetza_integration/context/rx_bridge/rx_bridge_context.h"

namespace ns3
{

namespace
{
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
            "SPATEM PKI configuration is incomplete");
    }

    return path;
}
} // namespace

struct SpatemV3VerifyService::Impl
{
    std::unique_ptr<vanetza::security::Backend> backend;
    vanetza::security::v3::CertificateCache certificateCache;

    vanetza::security::v3::TrustStore trustStore;
    vanetza::security::v3::IssuerMemoryLookup issuerLookup;
    vanetza::security::v3::DefaultLocationChecker locationChecker;
    vanetza::security::v3::DefaultCertificateValidator certificateValidator;

    std::unique_ptr<vanetza::security::StraightVerifyService> verifier;

    Impl(
        const vanetza::Runtime& runtime,
        vanetza::PositionProvider& position,
        const std::string& pkiDirectory)
    {
        if (pkiDirectory.empty())
        {
            throw std::runtime_error(
                "SPATEM PKI directory is required");
        }

        const std::filesystem::path directory(pkiDirectory);
        const auto rootPath =
            RequirePkiFile(directory, "spatem-root.cert");
        const auto aaPath =
            RequirePkiFile(directory, "spatem-aa.cert");

        const auto root =
            vanetza::security::v3::load_certificate_from_file(
                rootPath.string());
        const auto aa =
            vanetza::security::v3::load_certificate_from_file(
                aaPath.string());

        if (!issuerLookup.insert(aa) ||
            !issuerLookup.insert(root))
        {
            throw std::runtime_error(
                "SPATEM PKI issuer configuration is invalid");
        }

        // Trust is anchored only at the configured root certificate.
        trustStore.insert(root);

        backend =
            vanetza::security::create_backend("default");

        if (!backend)
        {
            throw std::runtime_error(
                "Unable to create SPATEM security backend");
        }

        // Do not disable temporal or geographic validation. If required
        // validation inputs are missing, initialization/verification must
        // fail rather than silently downgrade the policy.
        certificateValidator.use_runtime(&runtime);
        certificateValidator.use_position_provider(&position);
        certificateValidator.use_location_checker(&locationChecker);
        certificateValidator.use_issuer_lookup(&issuerLookup);
        certificateValidator.use_trust_store(&trustStore);

        verifier =
            std::make_unique<
                vanetza::security::StraightVerifyService>(
                    runtime,
                    *backend,
                    position);

        verifier->use_certificate_cache(&certificateCache);
        verifier->use_certificate_validator(&certificateValidator);
    }
};

SpatemV3VerifyService::SpatemV3VerifyService(
    const vanetza::Runtime& runtime,
    vanetza::PositionProvider& position,
    const std::string& pkiDirectory)
    : m_impl(std::make_unique<Impl>(
          runtime, position, pkiDirectory))
{
}

SpatemV3VerifyService::~SpatemV3VerifyService() = default;

vanetza::security::VerifyConfirm
SpatemV3VerifyService::verify(
    const vanetza::security::VerifyRequest& request)
{
    const auto requestedAid =
        vanetza::security::get_its_aid(request.secured_message);

    if (requestedAid != static_cast<vanetza::ItsAid>(137))
    {
        auto confirm = vanetza::security::VerifyConfirm{
            vanetza::security::VerificationReport::Configuration_Problem,
            requestedAid,
            {},
            vanetza::security::CertificateValidity::valid(),
            {}
        };

        PacketContext* context = RxBridgeContext::Current();
        if (context != nullptr)
        {
            context->verificationPerformed = true;
            context->verificationSucceeded = false;
            context->verificationReport =
                static_cast<std::uint32_t>(confirm.report);
            context->verifiedItsAid =
                static_cast<std::uint32_t>(requestedAid);
        }
        return confirm;
    }

    // Obtain the real cryptographic verdict from StraightVerifyService.
    auto confirm = m_impl->verifier->verify(request);

    const auto aid =
        vanetza::security::get_its_aid(
            request.secured_message);

    PacketContext* context = RxBridgeContext::Current();

    if (context != nullptr)
    {
        context->verificationPerformed = true;
        context->verificationSucceeded =
            confirm.report ==
            vanetza::security::VerificationReport::Success;
        context->verificationReport =
            static_cast<std::uint32_t>(confirm.report);
        context->verifiedItsAid =
            static_cast<std::uint32_t>(aid);
    }
    else
    {
        NS_LOG_UNCOND(
            "[SPATEM_V3_VERIFY] ERROR RxBridgeContext is null");
    }

    NS_LOG_UNCOND(
        "[SPATEM_V3_VERIFY]"
        << " aid=" << aid
        << " report=" << static_cast<unsigned>(confirm.report)
        << " cryptographic="
        << (confirm.report ==
                    vanetza::security::VerificationReport::Success
                ? "PASS"
                : "FAIL")
        << " context="
        << (context != nullptr ? "BOUND" : "NULL"));

    return confirm;
}

} // namespace ns3
