#include "denm_v3_verify_service.h"

#include <cstdint>
#include <filesystem>
#include <stdexcept>

#include <ns3/log.h>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/common/runtime.hpp>
#include <vanetza/security/backend.hpp>
#include <vanetza/security/certificate_validity.hpp>
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
constexpr vanetza::ItsAid kCamItsAid = 36;
constexpr vanetza::ItsAid kDenmItsAid = 37;

std::filesystem::path
RequirePkiFile(const std::filesystem::path& directory, const char* filename)
{
    const auto path = directory / filename;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec)
    {
        throw std::runtime_error(
            "vehicle CAM/DENM PKI configuration is incomplete");
    }
    return path;
}

vanetza::security::VerifyConfirm
MakeConfigurationFailure(vanetza::ItsAid aid)
{
    return {
        vanetza::security::VerificationReport::Configuration_Problem,
        aid,
        {},
        vanetza::security::CertificateValidity::valid(),
        {}
    };
}

void
RecordVerification(
    vanetza::ItsAid aid,
    vanetza::security::VerificationReport report)
{
    PacketContext* context = RxBridgeContext::Current();
    if (context == nullptr)
    {
        NS_LOG_UNCOND(
            "[VEHICLE_V3_VERIFY] ERROR RxBridgeContext is null");
        return;
    }

    context->verificationPerformed = true;
    context->verificationSucceeded =
        report == vanetza::security::VerificationReport::Success;
    context->verificationReport = static_cast<std::uint32_t>(report);
    context->verifiedItsAid = static_cast<std::uint32_t>(aid);
}
} // namespace

struct DenmV3VerifyService::Impl
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
                "vehicle CAM/DENM PKI directory is required");
        }

        const std::filesystem::path directory(pkiDirectory);
        const auto rootPath = RequirePkiFile(directory, "denm-root.cert");
        const auto aaPath = RequirePkiFile(directory, "denm-aa.cert");

        const auto root =
            vanetza::security::v3::load_certificate_from_file(rootPath.string());
        const auto aa =
            vanetza::security::v3::load_certificate_from_file(aaPath.string());

        if (!issuerLookup.insert(aa) || !issuerLookup.insert(root))
        {
            throw std::runtime_error(
                "vehicle CAM/DENM PKI issuer configuration is invalid");
        }

        // Only the configured root is trusted. The AA is an issuer, not
        // an independent trust anchor.
        trustStore.insert(root);

        backend = vanetza::security::create_backend("default");
        if (!backend)
        {
            throw std::runtime_error(
                "Unable to create vehicle V3 security backend");
        }

        // Keep default temporal and geographic checks enabled. Missing
        // runtime/position inputs must not result in a permissive downgrade.
        certificateValidator.use_runtime(&runtime);
        certificateValidator.use_position_provider(&position);
        certificateValidator.use_location_checker(&locationChecker);
        certificateValidator.use_issuer_lookup(&issuerLookup);
        certificateValidator.use_trust_store(&trustStore);

        verifier =
            std::make_unique<vanetza::security::StraightVerifyService>(
                runtime, *backend, position);
        verifier->use_certificate_cache(&certificateCache);
        verifier->use_certificate_validator(&certificateValidator);
    }
};

DenmV3VerifyService::DenmV3VerifyService(
    const vanetza::Runtime& runtime,
    vanetza::PositionProvider& position,
    const std::string& pkiDirectory)
    : m_impl(std::make_unique<Impl>(runtime, position, pkiDirectory))
{
}

DenmV3VerifyService::~DenmV3VerifyService() = default;

vanetza::security::VerifyConfirm
DenmV3VerifyService::verify(
    const vanetza::security::VerifyRequest& request)
{
    const vanetza::ItsAid aid =
        vanetza::security::get_its_aid(request.secured_message);

    if (aid != kCamItsAid && aid != kDenmItsAid)
    {
        RecordVerification(
            aid,
            vanetza::security::VerificationReport::Configuration_Problem);
        NS_LOG_UNCOND(
            "[VEHICLE_V3_VERIFY] REJECT unsupported_its_aid=" << aid);
        return MakeConfigurationFailure(aid);
    }

    auto confirm = m_impl->verifier->verify(request);
    RecordVerification(aid, confirm.report);

    NS_LOG_UNCOND(
        "[VEHICLE_V3_VERIFY]"
        << " aid=" << aid
        << " report=" << static_cast<unsigned>(confirm.report)
        << " cryptographic="
        << (confirm.report == vanetza::security::VerificationReport::Success
                ? "PASS"
                : "FAIL"));
    return confirm;
}

} // namespace ns3
