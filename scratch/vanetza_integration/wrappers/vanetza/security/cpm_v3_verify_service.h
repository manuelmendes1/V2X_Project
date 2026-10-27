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

class CpmV3VerifyService final : public vanetza::security::VerifyService
{
public:
    CpmV3VerifyService(
        const vanetza::Runtime& runtime,
        vanetza::PositionProvider& position,
        const std::string& pkiDirectory);

    ~CpmV3VerifyService() override;

    CpmV3VerifyService(const CpmV3VerifyService&) = delete;
    CpmV3VerifyService& operator=(const CpmV3VerifyService&) = delete;

    vanetza::security::VerifyConfirm verify(
        const vanetza::security::VerifyRequest& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ns3
