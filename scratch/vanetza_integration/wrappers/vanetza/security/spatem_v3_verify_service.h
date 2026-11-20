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

class SpatemV3VerifyService final : public vanetza::security::VerifyService
{
public:
    SpatemV3VerifyService(
        const vanetza::Runtime& runtime,
        vanetza::PositionProvider& position,
        const std::string& pkiDirectory);

    ~SpatemV3VerifyService() override;

    SpatemV3VerifyService(const SpatemV3VerifyService&) = delete;
    SpatemV3VerifyService& operator=(const SpatemV3VerifyService&) = delete;

    vanetza::security::VerifyConfirm verify(
        const vanetza::security::VerifyRequest& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ns3
