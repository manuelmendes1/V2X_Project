#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential_pool.h"
#include "vanetza_integration/context/vehicle_security/file_credential_material_resolver.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential_binding_validator.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_sign_service.h"

#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/net/packet.hpp>
#include <vanetza/security/sign_service.hpp>
#include <vanetza/security/v3/certificate_validator.hpp>
#include <vanetza/security/v3/persistence.hpp>
#include <vanetza/security/v3/secured_message.hpp>

#include <boost/optional/optional.hpp>
#include <boost/variant/get.hpp>

#include <cstdlib>
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

boost::optional<vanetza::security::HashedId8>
GetCertificateId(
    const vanetza::security::SignConfirm& confirm)
{
    if (!confirm.secured_message)
    {
        return boost::none;
    }

    const auto* message =
        boost::get<vanetza::security::v3::SecuredMessage>(
            &*confirm.secured_message);

    if (message == nullptr)
    {
        return boost::none;
    }

    return vanetza::security::v3::get_certificate_id(
        message->signer_identifier());
}

} // namespace

int
main()
{
    std::cout
        << "===== DENM V3 ROTATION POLICY TEST =====\n";

    const char* pkiEnv =
        std::getenv("DENM_TEST_PKI_DIR");

    const std::string pkiDir =
        pkiEnv != nullptr ? pkiEnv : "";

    if (pkiDir.empty())
    {
        std::cerr
            << "FAIL DENM_TEST_PKI_DIR is not configured\n";
        return 1;
    }

    /*
     * veh0-ticket-000 remains valid at this instant.
     *
     * /tmp/p11-expired-ticket.cert was generated on
     * 2026-09-29 with --days 1 and is therefore expired here.
     */
    const auto itsStartTime =
        vanetza::Clock::at("2026-10-01 00:00:00");

    vanetza::ManualRuntime runtime(itsStartTime);
    TestPositionProvider position;
    ns3::VehicleCredentialPool credentialPool;

    const ns3::VehicleCredential credential000{
        "veh0-ticket-000",
        "veh0",
        0,
        pkiDir + "/veh0-ticket-000.cert",
        pkiDir + "/veh0-ticket-000.pem"};

    const ns3::VehicleCredential expiredCredential{
        "p11-expired-ticket",
        "veh0",
        1,
        "/tmp/p11-expired-ticket.cert",
        pkiDir + "/veh0-ticket-001.pem"};

    bool candidateBindingValid = false;
    bool candidateTemporalValid = true;
    vanetza::security::v3::Certificate candidateCertificate;

    try
    {
        candidateBindingValid =
            ns3::VehicleCredentialBindingValidator::Validate(
                expiredCredential);

        candidateCertificate =
            vanetza::security::v3::load_certificate_from_file(
                expiredCredential.certFile);

        candidateTemporalValid =
            candidateCertificate.valid_at_timepoint(
                runtime.now());
    }
    catch (...)
    {
        candidateBindingValid = false;
        candidateTemporalValid = true;
    }

    credentialPool.SetCredentials({
        credential000,
        expiredCredential});

    bool ok = true;

    ok &= Check(
        candidateBindingValid,
        "expired candidate certificate and private key are correctly bound");

    ok &= Check(
        !candidateTemporalValid,
        "expired candidate is temporally invalid at rotation time");

    ok &= Check(
        credentialPool.Select(0),
        "valid credential 000 is initially active");

    ns3::FileCredentialMaterialResolver materialResolver;

    ns3::DenmV3SignService signer(
        runtime,
        position,
        credentialPool,
        materialResolver);

    auto firstConfirm =
        signer.sign(MakeDenmSignRequest());

    ok &= Check(
        static_cast<bool>(firstConfirm.secured_message),
        "baseline DENM signing succeeds");

    const auto firstCertificateId =
        GetCertificateId(firstConfirm);

    ok &= Check(
        static_cast<bool>(firstCertificateId),
        "baseline signer exposes certificate identity");

    ok &= Check(
        credentialPool.Select(1),
        "expired candidate is selected for rotation attempt");

    /*
     * P11 contract:
     * a temporally invalid candidate must be rejected before
     * replacing the currently active signer.
     */
    ok &= Check(
        !signer.ReloadActiveCredential(),
        "expired DENM credential is rejected during rotation");

    /*
     * The failed rotation must not destroy the previous signer.
     * The pool selection itself is intentionally unchanged here;
     * P10 already proves runtime-level rollback.
     */
    auto preservedConfirm =
        signer.sign(MakeDenmSignRequest());

    ok &= Check(
        static_cast<bool>(preservedConfirm.secured_message),
        "previous DENM signer remains operational after rejection");

    const auto preservedCertificateId =
        GetCertificateId(preservedConfirm);

    ok &= Check(
        static_cast<bool>(preservedCertificateId),
        "preserved signer exposes certificate identity");

    if (firstCertificateId && preservedCertificateId)
    {
        ok &= Check(
            *firstCertificateId == *preservedCertificateId,
            "rejected temporal credential does not replace signer");
    }

    std::cout
        << "DENM_V3_ROTATION_POLICY_EXIT="
        << (ok ? 0 : 1)
        << '\n';

    return ok ? 0 : 1;
}
