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

class CpmV3SignService final : public vanetza::security::SignService
{
public:
    CpmV3SignService(
        vanetza::ManualRuntime& runtime,
        vanetza::PositionProvider& position,
        const std::string& pkiDirectory);

    ~CpmV3SignService() override;

    CpmV3SignService(const CpmV3SignService&) = delete;
    CpmV3SignService& operator=(const CpmV3SignService&) = delete;

    vanetza::security::SignConfirm sign(
        vanetza::security::SignRequest&& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ns3
