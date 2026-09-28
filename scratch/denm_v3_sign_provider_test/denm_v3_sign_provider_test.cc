#include "vanetza_integration/context/vehicle_security/active_credential_provider.h"
#include "vanetza_integration/context/vehicle_security/credential_material_resolver.h"
#include "vanetza_integration/context/vehicle_security/vehicle_credential_pool.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_sign_service.h"

#include <iostream>
#include <memory>
#include <string>

#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_provider.hpp>
#include <vanetza/security/key_type.hpp>
#include <vanetza/security/v3/certificate.hpp>

using namespace ns3;

namespace
{

class DummyPositionProvider : public vanetza::PositionProvider
{
public:
    const vanetza::PositionFix& position_fix() override
    {
        return m_fix;
    }

private:
    vanetza::PositionFix m_fix{};
};

class FakeCredentialMaterialResolver final
    : public CredentialMaterialResolver
{
public:
    std::unique_ptr<DenmV3CredentialMaterial>
    Resolve(const VehicleCredential& credential) const override
    {
        lastCredentialId = credential.credentialId;

        auto material =
            std::make_unique<DenmV3CredentialMaterial>();

        material->certificate =
            vanetza::security::v3::fake_certificate();

        material->privateKey.type =
            vanetza::security::KeyType::NistP256;

        material->privateKey.key.resize(32, 0x01);

        return material;
    }

    mutable std::string lastCredentialId;
};

int Fail(const std::string& message)
{
    std::cerr << "FAIL " << message << std::endl;
    return 1;
}

void Pass(const std::string& message)
{
    std::cout << "PASS " << message << std::endl;
}

} // namespace

int main()
{
    vanetza::ManualRuntime runtime;
    DummyPositionProvider position;

    VehicleCredentialPool pool;

    pool.SetCredentials({
        VehicleCredential{
            "veh0-ticket-000",
            "veh0",
            0,
            "/invalid/certificate.cert",
            "/invalid/private-key.pem"},
        VehicleCredential{
            "veh0-ticket-001",
            "veh0",
            1,
            "/also-invalid/certificate.cert",
            "/also-invalid/private-key.pem"}});

    if (!pool.Select(1))
    {
        return Fail("test pool selects credential[1]");
    }

    const ActiveCredentialProvider& provider = pool;
    FakeCredentialMaterialResolver resolver;

    try
    {
        DenmV3SignService signer(
            runtime,
            position,
            provider,
            resolver);

        (void)signer;
    }
    catch (const std::exception& error)
    {
        return Fail(
            std::string("signer construction failed: ") +
            error.what());
    }

    if (resolver.lastCredentialId != "veh0-ticket-001")
    {
        return Fail(
            "signer did not consume the provider's active credential");
    }

    Pass(
        "signer consumes the active credential through the provider");

    Pass(
        "signer accepts credential material without filesystem access");

    std::cout
        << "DENM_V3_SIGN_PROVIDER_TEST_EXIT=0"
        << std::endl;

    return 0;
}
