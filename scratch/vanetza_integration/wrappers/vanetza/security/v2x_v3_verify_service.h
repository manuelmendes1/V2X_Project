#pragma once

#include <memory>
#include <string>

#include <vanetza/security/verify_service.hpp>

namespace vanetza
{
class PositionProvider;
class Runtime;
}

namespace ns3
{

class V2xV3VerifyService final : public vanetza::security::VerifyService
{
public:
    V2xV3VerifyService(
        const vanetza::Runtime& runtime,
        vanetza::PositionProvider& position,
        const std::string& cpmPkiDirectory,
        const std::string& vehiclePkiRunDirectory = {});

    ~V2xV3VerifyService() override;

    V2xV3VerifyService(const V2xV3VerifyService&) = delete;
    V2xV3VerifyService& operator=(const V2xV3VerifyService&) = delete;

    vanetza::security::VerifyConfirm verify(
        const vanetza::security::VerifyRequest& request) override;

private:
    std::unique_ptr<vanetza::security::VerifyService> m_denmVerifier;
    std::unique_ptr<vanetza::security::VerifyService> m_cpmVerifier;
};

} // namespace ns3
