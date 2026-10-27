#include "v2x_v3_verify_service.h"

#include <cstdint>
#include <memory>

#include <ns3/log.h>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/common/runtime.hpp>
#include <vanetza/security/certificate_validity.hpp>
#include <vanetza/security/verify_service.hpp>

#include "vanetza_integration/context/rx_bridge/rx_bridge_context.h"
#include "vanetza_integration/wrappers/vanetza/security/cpm_v3_verify_service.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_verify_service.h"

namespace ns3
{

namespace
{
constexpr vanetza::ItsAid kCpmItsAid = 639;

vanetza::security::VerifyConfirm
MakeCpmConfigurationFailure()
{
    // Initialize every mandatory VerifyConfirm field. The report is a
    // failure regardless of the placeholder certificate-validity field.
    return {
        vanetza::security::VerificationReport::Configuration_Problem,
        kCpmItsAid,
        {},
        vanetza::security::CertificateValidity::valid(),
        {}
    };
}

void
RecordCpmConfigurationFailure()
{
    PacketContext* context = RxBridgeContext::Current();

    if (context == nullptr)
    {
        NS_LOG_UNCOND(
            "[V2X_V3_VERIFY] CPM rejected: RxBridgeContext is null");
        return;
    }

    context->verificationPerformed = true;
    context->verificationSucceeded = false;
    context->verificationReport = static_cast<std::uint32_t>(
        vanetza::security::VerificationReport::Configuration_Problem);
    context->verifiedItsAid = static_cast<std::uint32_t>(kCpmItsAid);
}
} // namespace

V2xV3VerifyService::V2xV3VerifyService(
    const vanetza::Runtime& runtime,
    vanetza::PositionProvider& position,
    const std::string& cpmPkiDirectory)
    : m_denmVerifier(
          std::make_unique<DenmV3VerifyService>(runtime))
{
    // An empty path is allowed for runs that do not configure CPM.
    // A non-empty but invalid directory must throw; it never downgrades
    // CPM verification to the legacy verifier.
    if (!cpmPkiDirectory.empty())
    {
        m_cpmVerifier = std::make_unique<CpmV3VerifyService>(
            runtime, position, cpmPkiDirectory);
    }
}

V2xV3VerifyService::~V2xV3VerifyService() = default;

vanetza::security::VerifyConfirm
V2xV3VerifyService::verify(
    const vanetza::security::VerifyRequest& request)
{
    const auto aid =
        vanetza::security::get_its_aid(request.secured_message);

    if (aid == kCpmItsAid)
    {
        if (m_cpmVerifier)
        {
            return m_cpmVerifier->verify(request);
        }

        // Never route AID 639 through DenmV3VerifyService's compatibility
        // fallback, which accepts non-DENM AIDs without real verification.
        RecordCpmConfigurationFailure();

        NS_LOG_UNCOND(
            "[V2X_V3_VERIFY] CPM rejected: no CPM PKI verifier configured");

        return MakeCpmConfigurationFailure();
    }

    // Keep the existing DENM and non-CPM verification behavior unchanged.
    return m_denmVerifier->verify(request);
}

} // namespace ns3
