#include "vanetza_integration/context/vehicle_security/denm_credential.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_sign_service.h"

#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/net/packet.hpp>
#include <vanetza/security/sign_service.hpp>
#include <vanetza/security/v3/secured_message.hpp>

#include <boost/variant/get.hpp>

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
        std::cerr << "FAIL " << message << "\n";
        return false;
    }

    std::cout << "PASS " << message << "\n";
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

} // namespace

int
main()
{
    std::cout
        << "===== DENM V3 CREDENTIAL ROTATION RED TEST =====\n";

    const char* pkiEnv =
        std::getenv("DENM_TEST_PKI_DIR");

    if (pkiEnv == nullptr || std::string(pkiEnv).empty())
    {
        std::cerr
            << "FAIL DENM_TEST_PKI_DIR is not set\n";
        return 1;
    }

    const ns3::DenmCredential credential000{
        pkiEnv + "/veh0-ticket-000.cert",
        pkiEnv + "/veh0-ticket-000.pem"};

    const ns3::DenmCredential credential001{
        pkiEnv + "/veh0-ticket-001.cert",
        pkiEnv + "/veh0-ticket-001.pem"};

    vanetza::ManualRuntime runtime;
    TestPositionProvider position;

    ns3::DenmV3SignService signer(
        runtime,
        position,
        credential000.certFile,
        credential000.keyFile);

    bool ok = true;

    // ------------------------------------------------------------
    // Initial credential
    // ------------------------------------------------------------
    auto firstConfirm =
        signer.sign(MakeDenmSignRequest());

    ok &= Check(
        static_cast<bool>(firstConfirm.secured_message),
        "first DENM signature succeeds");

    boost::optional<vanetza::security::HashedId8>
        firstCertificateId;

    if (firstConfirm.secured_message)
    {
        const auto* firstV3Message =
            boost::get<vanetza::security::v3::SecuredMessage>(
                &*firstConfirm.secured_message);

        ok &= Check(
            firstV3Message != nullptr,
            "first signature produced a V3 secured message");

        if (firstV3Message != nullptr)
        {
            firstCertificateId =
                vanetza::security::v3::get_certificate_id(
                    firstV3Message->signer_identifier());

            ok &= Check(
                static_cast<bool>(firstCertificateId),
                "first signature exposes certificate identity");
        }
    }

    // ------------------------------------------------------------
    // Rotation
    // ------------------------------------------------------------
    ok &= Check(
        signer.SetActiveCredential(credential001),
        "second DENM credential can be applied to signer");

    auto secondConfirm =
        signer.sign(MakeDenmSignRequest());

    ok &= Check(
        static_cast<bool>(secondConfirm.secured_message),
        "second DENM signature succeeds after rotation");

    boost::optional<vanetza::security::HashedId8>
        secondCertificateId;

    if (secondConfirm.secured_message)
    {
        const auto* secondV3Message =
            boost::get<vanetza::security::v3::SecuredMessage>(
                &*secondConfirm.secured_message);

        ok &= Check(
            secondV3Message != nullptr,
            "second signature produced a V3 secured message");

        if (secondV3Message != nullptr)
        {
            secondCertificateId =
                vanetza::security::v3::get_certificate_id(
                    secondV3Message->signer_identifier());

            ok &= Check(
                static_cast<bool>(secondCertificateId),
                "second signature exposes certificate identity");
        }
    }

    if (firstCertificateId && secondCertificateId)
    {
        ok &= Check(
            *firstCertificateId != *secondCertificateId,
            "credential rotation changes the DENM signer certificate");
    }

    std::cout
        << "DENM_V3_CREDENTIAL_ROTATION_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
