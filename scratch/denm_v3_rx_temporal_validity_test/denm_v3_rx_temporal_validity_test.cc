#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential_pool.h"
#include "vanetza_integration/context/vehicle_security/file_credential_material_resolver.h"
#include "vanetza_integration/tests/denm_test_pki.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_sign_service.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_verify_service.h"

#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/net/packet.hpp>
#include <vanetza/security/sign_service.hpp>
#include <vanetza/security/v3/secured_message.hpp>
#include <vanetza/security/verify_service.hpp>

#include <chrono>
#include <iostream>
#include <string>

namespace
{

class TestPositionProvider final : public vanetza::PositionProvider
{
public:
    const vanetza::PositionFix&
    position_fix() override
    {
        return m_fix;
    }

private:
    vanetza::PositionFix m_fix;
};

bool
Check(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }

    std::cout << "PASS " << message << '\n';
    return true;
}

vanetza::security::SignRequest
MakeDenmSignRequest()
{
    vanetza::security::SignRequest request;

    request.its_aid = vanetza::aid::DEN;

    vanetza::ByteBuffer payload{
        0x01,
        0x02,
        0x03,
        0x04};

    request.plain_message.layer(
        vanetza::OsiLayer::Application) =
        std::move(payload);

    return request;
}

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

int
main()
{
    std::cout
        << "===== DENM V3 RX TEMPORAL VALIDITY TEST =====\n";

    const std::string pkiDir =
        ns3::denm_test::ResolveDenmPkiDirectory();

    if (pkiDir.empty())
    {
        std::cerr
            << "FAIL DENM test PKI directory is unavailable\n";
        return 1;
    }

    const ns3::VehicleCredential credential000{
        "veh0-ticket-000",
        "veh0",
        0,
        pkiDir + "/veh0-ticket-000.cert",
        pkiDir + "/veh0-ticket-000.pem"};

    const auto itsStartTime =
        vanetza::Clock::at("2026-09-24 10:01:32");

    const auto expiryTime =
        itsStartTime +
        std::chrono::hours(168) +
        std::chrono::seconds(1);

    vanetza::ManualRuntime runtime(itsStartTime);
    TestPositionProvider position;

    ns3::VehicleCredentialPool credentialPool;
    credentialPool.SetCredentials({
        credential000});

    bool ok = true;

    ok &= Check(
        credentialPool.Select(0),
        "credential 000 is selected for DENM signing");

    ns3::FileCredentialMaterialResolver materialResolver;

    ns3::DenmV3SignService signer(
        runtime,
        position,
        credentialPool,
        materialResolver);

    auto signConfirm =
        signer.sign(MakeDenmSignRequest());

    ok &= Check(
        static_cast<bool>(signConfirm.secured_message),
        "valid DENM V3 message is signed before expiry");

    if (!signConfirm.secured_message)
    {
        std::cout
            << "DENM_V3_RX_TEMPORAL_VALIDITY_TEST_EXIT=1\n";
        return 1;
    }

    ns3::DenmV3VerifyService verifier(runtime);

    const auto beforeExpiryConfirm =
        verifier.verify(
            vanetza::security::VerifyRequest{
                vanetza::security::SecuredMessageView{
                    *signConfirm.secured_message}});

    std::cout
        << "FIRST_VERIFY_REPORT="
        << VerificationReportToString(beforeExpiryConfirm.report)
        << '\n';

    ok &= Check(
        beforeExpiryConfirm.report ==
            vanetza::security::VerificationReport::Success,
        "DENM V3 verification succeeds while credential is valid");

    runtime.trigger(expiryTime);

    ok &= Check(
        runtime.now() == expiryTime,
        "receiver runtime reaches the post-expiry time");

    const auto afterExpiryConfirm =
        verifier.verify(
            vanetza::security::VerifyRequest{
                vanetza::security::SecuredMessageView{
                    *signConfirm.secured_message}});

    std::cout
        << "EXPIRED_VERIFY_REPORT="
        << VerificationReportToString(afterExpiryConfirm.report)
        << '\n';

    ok &= Check(
        afterExpiryConfirm.report ==
            vanetza::security::VerificationReport::Invalid_Certificate,
        "DENM V3 verification rejects the expired certificate");

    ok &= Check(
        afterExpiryConfirm.report !=
            vanetza::security::VerificationReport::Success,
        "expired DENM V3 certificate is not accepted by RX");

    std::cout
        << "DENM_V3_RX_TEMPORAL_VALIDITY_TEST_EXIT="
        << (ok ? 0 : 1)
        << '\n';

    return ok ? 0 : 1;
}
