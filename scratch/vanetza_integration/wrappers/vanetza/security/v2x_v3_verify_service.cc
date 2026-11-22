#include "v2x_v3_verify_service.h"

#include <cstdint>

#include <ns3/log.h>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/common/runtime.hpp>
#include <vanetza/security/certificate_validity.hpp>
#include <vanetza/security/verify_service.hpp>

#include "vanetza_integration/context/rx_bridge/rx_bridge_context.h"
#include "vanetza_integration/wrappers/vanetza/security/cpm_v3_verify_service.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_verify_service.h"
#include "vanetza_integration/wrappers/vanetza/security/spatem_v3_verify_service.h"
#include "vanetza_integration/wrappers/vanetza/security/v2x_v3_security_profile.h"

namespace ns3
{
namespace
{

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
RecordConfigurationFailure(vanetza::ItsAid aid)
{
    PacketContext* context = RxBridgeContext::Current();
    if (context == nullptr)
    {
        NS_LOG_UNCOND(
            "[V2X_V3_VERIFY] rejected AID " << aid
            << ": RxBridgeContext is null");
        return;
    }

    context->verificationPerformed = true;
    context->verificationSucceeded = false;
    context->verificationReport = static_cast<std::uint32_t>(
        vanetza::security::VerificationReport::Configuration_Problem);
    context->verifiedItsAid = static_cast<std::uint32_t>(aid);
}
} // namespace

V2xV3VerifyService::V2xV3VerifyService(
    const vanetza::Runtime& runtime,
    vanetza::PositionProvider& position,
    const std::string& vehiclePkiDirectory,
    const std::string& spatemPkiDirectory,
    const std::string& cpmPkiDirectory)
{
    // Distinct protocol PKIs remain distinct trust domains. An absent domain
    // leaves only its AIDs unavailable; it never selects a permissive verifier.
    if (!vehiclePkiDirectory.empty())
    {
        m_vehicleVerifier =
            std::make_unique<DenmV3VerifyService>(
                runtime, position, vehiclePkiDirectory);
    }
    if (!spatemPkiDirectory.empty())
    {
        m_spatemVerifier =
            std::make_unique<SpatemV3VerifyService>(
                runtime, position, spatemPkiDirectory);
    }
    if (!cpmPkiDirectory.empty())
    {
        m_cpmVerifier =
            std::make_unique<CpmV3VerifyService>(
                runtime, position, cpmPkiDirectory);
    }
}

V2xV3VerifyService::~V2xV3VerifyService() = default;

vanetza::security::VerifyConfirm
V2xV3VerifyService::verify(
    const vanetza::security::VerifyRequest& request)
{
    const vanetza::ItsAid aid =
        vanetza::security::get_its_aid(request.secured_message);

    switch (ClassifyV2xV3SecurityProfile(
        static_cast<std::uint32_t>(aid)))
    {
    case V2xV3SecurityProfile::VehicleCamDenm:
        if (m_vehicleVerifier)
        {
            return m_vehicleVerifier->verify(request);
        }
        break;
    case V2xV3SecurityProfile::SpatemRsu:
        if (m_spatemVerifier)
        {
            return m_spatemVerifier->verify(request);
        }
        break;
    case V2xV3SecurityProfile::CpmRsu:
        if (m_cpmVerifier)
        {
            return m_cpmVerifier->verify(request);
        }
        break;
    case V2xV3SecurityProfile::Unsupported:
        break;
    }

    // Unknown AIDs and known AIDs without an explicitly configured trust
    // domain are configuration failures, never DummyVerifyService success.
    RecordConfigurationFailure(aid);
    NS_LOG_UNCOND(
        "[V2X_V3_VERIFY] REJECT aid=" << aid
        << " reason=unsupported_or_unconfigured_security_profile");
    return MakeConfigurationFailure(aid);
}

} // namespace ns3
