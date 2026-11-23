#pragma once

#include <vanetza/security/verify_service.hpp>

#include <memory>
#include <string>

namespace vanetza
{
class Runtime;

namespace security
{

class Backend;

namespace v3
{
class CertificateCache;
class DefaultCertificateValidator;
class CertificateProvider;
}

} // namespace security
} // namespace vanetza

namespace ns3
{

class DenmV3VerifyService final :
    public vanetza::security::VerifyService
{
public:
    explicit DenmV3VerifyService(
        const vanetza::Runtime& runtime,
        const std::string& vehiclePkiRunDirectory = {});

    ~DenmV3VerifyService();

    vanetza::security::VerifyConfirm verify(
        const vanetza::security::VerifyRequest& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ns3
