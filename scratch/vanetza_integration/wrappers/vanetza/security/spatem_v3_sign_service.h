#pragma once

#include <memory>
#include <string>

#include <vanetza/security/sign_service.hpp>

namespace vanetza
{
class ManualRuntime;
class PositionProvider;
}

namespace ns3
{

class SpatemV3SignService final : public vanetza::security::SignService
{
public:
    SpatemV3SignService(
        vanetza::ManualRuntime& runtime,
        vanetza::PositionProvider& position,
        const std::string& pkiDirectory);

    ~SpatemV3SignService() override;

    SpatemV3SignService(const SpatemV3SignService&) = delete;
    SpatemV3SignService& operator=(const SpatemV3SignService&) = delete;

    vanetza::security::SignConfirm sign(
        vanetza::security::SignRequest&& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ns3
