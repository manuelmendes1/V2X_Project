#include "denm_v3_verify_service.h"

#include <vanetza/common/its_aid.hpp>
#include <vanetza/common/runtime.hpp>
#include <vanetza/security/backend.hpp>
#include <vanetza/security/certificate_validity.hpp>
#include <vanetza/security/straight_verify_service.hpp>
#include <vanetza/security/verify_service.hpp>
#include <vanetza/security/v3/certificate_cache.hpp>
#include <vanetza/security/v3/certificate_validator.hpp>

#include <ns3/log.h>

#include "vanetza_integration/context/rx_bridge/rx_bridge_context.h"

#include <stdexcept>

namespace ns3
{

namespace
{

const char*
VerificationReportToString(
    vanetza::security::VerificationReport report)
{
    using vanetza::security::VerificationReport;

    switch (report)
    {
    case VerificationReport::Success:
        return "SUCCESS";

    case VerificationReport::False_Signature:
        return "FALSE_SIGNATURE";

    case VerificationReport::Invalid_Certificate:
        return "INVALID_CERTIFICATE";

    case VerificationReport::Revoked_Certificate:
        return "REVOKED_CERTIFICATE";

    case VerificationReport::Inconsistent_Chain:
        return "INCONSISTENT_CHAIN";

    case VerificationReport::Invalid_Timestamp:
        return "INVALID_TIMESTAMP";

    case VerificationReport::Duplicate_Message:
        return "DUPLICATE_MESSAGE";

    case VerificationReport::Invalid_Mobility_Data:
        return "INVALID_MOBILITY_DATA";

    case VerificationReport::Unsigned_Message:
        return "UNSIGNED_MESSAGE";

    case VerificationReport::Signer_Certificate_Not_Found:
        return "SIGNER_CERTIFICATE_NOT_FOUND";

    case VerificationReport::Unsupported_Signer_Identifier_Type:
        return "UNSUPPORTED_SIGNER_IDENTIFIER_TYPE";

    case VerificationReport::Incompatible_Protocol:
        return "INCOMPATIBLE_PROTOCOL";

    case VerificationReport::Configuration_Problem:
        return "CONFIGURATION_PROBLEM";
    }

    return "UNKNOWN";
}

} // namespace

struct DenmV3VerifyService::Impl
{
    std::unique_ptr<vanetza::security::Backend> backend;

    vanetza::security::v3::CertificateCache certificateCache;

    vanetza::security::v3::DefaultCertificateValidator certificateValidator;

    std::unique_ptr<vanetza::security::StraightVerifyService> realVerifier;

    std::unique_ptr<vanetza::security::DummyVerifyService> fallbackVerifier;

    explicit Impl(
        const vanetza::Runtime& runtime)
    {
        backend =
            vanetza::security::create_backend("default");

        if (!backend)
        {
            throw std::runtime_error(
                "Unable to create Vanetza default security backend");
        }

        // This stage intentionally isolates cryptographic verification
        // from certificate time/location policy validation.
        //
        // The certificate's ITS-AID permission is still checked by
        // DefaultCertificateValidator::valid_for_signing().
        // Cryptographic-validation stage:
        // keep AID permission validation, but do not bind the
        // certificate policy validator to ManualRuntime.
        certificateValidator.disable_time_checks(true);
        certificateValidator.disable_location_checks(true);

        realVerifier =
            std::make_unique<
                vanetza::security::StraightVerifyService>(
                    runtime,
                    *backend);

        realVerifier->use_certificate_cache(
            &certificateCache);

        realVerifier->use_certificate_validator(
            &certificateValidator);

        // Preserve existing CAM / non-DENM behavior.
        fallbackVerifier =
            std::make_unique<
                vanetza::security::DummyVerifyService>(
                    vanetza::security::VerificationReport::Success,
                    vanetza::security::CertificateValidity::valid());
    }
};

DenmV3VerifyService::DenmV3VerifyService(
    const vanetza::Runtime& runtime) :
    m_impl(std::make_unique<Impl>(runtime))
{
}

DenmV3VerifyService::~DenmV3VerifyService() = default;

vanetza::security::VerifyConfirm
DenmV3VerifyService::verify(
    const vanetza::security::VerifyRequest& request)
{
    const vanetza::ItsAid aid =
        vanetza::security::get_its_aid(
            request.secured_message);

    NS_LOG_UNCOND(
        "[DENM_V3_VERIFY_ENTRY] aid="
        << aid);

    //----------------------------------------------------------------------
    // DENM / AID 37
    //
    // Real ETSI Security V3 verification.
    //----------------------------------------------------------------------
    if (aid == 37)
    {
        auto confirm =
            m_impl->realVerifier->verify(request);

        PacketContext* ctx =
            RxBridgeContext::Current();

        if (ctx)
        {
            ctx->verificationPerformed = true;
            ctx->verificationSucceeded =
                confirm.report ==
                vanetza::security::VerificationReport::Success;

            ctx->verificationReport =
                static_cast<uint32_t>(confirm.report);

            ctx->verifiedItsAid =
                static_cast<uint32_t>(aid);
        }
        else
        {
            NS_LOG_UNCOND(
                "[DENM_V3_VERIFY] ERROR "
                "RxBridgeContext is null");
        }

        NS_LOG_UNCOND(
            "[DENM_V3_VERIFY] "
            "protocol_version=3 "
            "aid=37 "
            "report="
            << VerificationReportToString(confirm.report)
            << " "
            "cryptographic="
            << (confirm.report ==
                    vanetza::security::VerificationReport::Success
                    ? "PASS"
                    : "FAIL")
            << " "
            "context="
            << (ctx ? "BOUND" : "NULL"));

        return confirm;
    }

    //----------------------------------------------------------------------
    // CAM / other AIDs
    //
    // Keep the existing dummy behavior for the current baseline.
    //----------------------------------------------------------------------
    return m_impl->fallbackVerifier->verify(request);
}

} // namespace ns3
