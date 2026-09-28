#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential_pool.h"
#include "vanetza_integration/context/vehicle_security/file_credential_material_resolver.h"
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
#include "vanetza_integration/tests/denm_test_pki.h"

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
        << "===== DENM V3 CREDENTIAL ROTATION TEST =====\n";

    const std::string pkiDir =
        ns3::denm_test::ResolveDenmPkiDirectory();

    if (pkiDir.empty())
    {
        std::cerr
            << "FAIL DENM test PKI directory is unavailable; "
               "set DENM_TEST_PKI_DIR or provide "
               "$HOME/vanetza-pki-lab/validation-denm-pki\n";
        return 1;
    }

    const ns3::VehicleCredential credential000{
        "veh0-ticket-000",
        "veh0",
        0,
        pkiDir + "/veh0-ticket-000.cert",
        pkiDir + "/veh0-ticket-000.pem"};

    const ns3::VehicleCredential credential001{
        "veh0-ticket-001",
        "veh0",
        1,
        pkiDir + "/veh0-ticket-001.cert",
        pkiDir + "/veh0-ticket-001.pem"};

    const ns3::VehicleCredential credentialMismatch{
        "veh0-ticket-mismatch",
        "veh0",
        1,
        pkiDir + "/veh0-ticket-000.cert",
        pkiDir + "/veh0-ticket-001.pem"};

    // The DENM test PKI starts at 2026-09-24 10:01:32 UTC.
    // The signer now enforces certificate temporal validity, so the
    // test runtime must start inside the credential validity window.
    const auto itsStartTime =
        vanetza::Clock::at("2026-09-24 10:01:32");

    vanetza::ManualRuntime runtime(itsStartTime);
    TestPositionProvider position;

    ns3::VehicleCredentialPool credentialPool;

    credentialPool.SetCredentials({
        credential000,
        credentialMismatch,
        credential001});

    if (!credentialPool.Select(0))
    {
        std::cerr << "FAIL initial credential selection\n";
        return 1;
    }

    ns3::FileCredentialMaterialResolver materialResolver;

    ns3::DenmV3SignService signer(
        runtime,
        position,
        credentialPool,
        materialResolver);

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
    // Invalid binding must be rejected without replacing the
    // currently active signer.
    // ------------------------------------------------------------
    ok &= Check(
        credentialPool.Select(1),
        "mismatched credential can be selected for reload");

    ok &= Check(
        !signer.ReloadActiveCredential(),
        "certificate/private key mismatch is rejected");

    auto preservedConfirm =
        signer.sign(MakeDenmSignRequest());

    ok &= Check(
        static_cast<bool>(preservedConfirm.secured_message),
        "DENM signature still succeeds after rejected credential");

    boost::optional<vanetza::security::HashedId8>
        preservedCertificateId;

    if (preservedConfirm.secured_message)
    {
        const auto* preservedV3Message =
            boost::get<vanetza::security::v3::SecuredMessage>(
                &*preservedConfirm.secured_message);

        ok &= Check(
            preservedV3Message != nullptr,
            "rejected credential leaves a V3 signer active");

        if (preservedV3Message != nullptr)
        {
            preservedCertificateId =
                vanetza::security::v3::get_certificate_id(
                    preservedV3Message->signer_identifier());

            ok &= Check(
                static_cast<bool>(preservedCertificateId),
                "preserved signature exposes certificate identity");
        }
    }

    if (firstCertificateId && preservedCertificateId)
    {
        ok &= Check(
            *firstCertificateId == *preservedCertificateId,
            "rejected credential preserves the previous signer certificate");
    }

    // ------------------------------------------------------------
    // Rotation
    // ------------------------------------------------------------
    ok &= Check(
        credentialPool.Select(2),
        "second DENM credential can be selected");

    ok &= Check(
        signer.ReloadActiveCredential(),
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
