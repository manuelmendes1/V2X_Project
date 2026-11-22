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

// Shared vehicle verifier for CAM (AID 36) and DENM (AID 37).
// The legacy class name is retained to avoid unnecessary source churn.
class DenmV3VerifyService final :
    public vanetza::security::VerifyService
{
public:
    DenmV3VerifyService(
        const vanetza::Runtime& runtime,
        vanetza::PositionProvider& position,
        const std::string& pkiDirectory);

    ~DenmV3VerifyService() override;

    DenmV3VerifyService(const DenmV3VerifyService&) = delete;
    DenmV3VerifyService& operator=(const DenmV3VerifyService&) = delete;

    vanetza::security::VerifyConfirm verify(
        const vanetza::security::VerifyRequest& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ns3
