#pragma once

#include <memory>
#include <string>

#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_provider.hpp>

#include <vanetza/security/backend.hpp>
#include <vanetza/security/sign_service.hpp>

#include <vanetza/security/v3/certificate_provider.hpp>
#include <vanetza/security/v3/certificate_validator.hpp>
#include <vanetza/security/v3/sign_header_policy.hpp>
#include <vanetza/security/v3/sign_service.hpp>
#include <vanetza/security/v3/static_certificate_provider.hpp>

namespace ns3 {

    class DenmV3SignService final : public vanetza::security::SignService {

        public:

            DenmV3SignService(vanetza::ManualRuntime& runtime, vanetza::PositionProvider& position, const std::string& certificateFile, const std::string& privateKeyFile);

            vanetza::security::SignConfirm sign(vanetza::security::SignRequest&& request) override;

        private:

            std::unique_ptr<vanetza::security::Backend> m_backend;

            vanetza::security::v3::StaticCertificateProvider m_certificateProvider;

            vanetza::security::v3::DefaultSignHeaderPolicy m_signHeaderPolicy;

            vanetza::security::v3::DefaultCertificateValidator m_certificateValidator;

            vanetza::security::v3::StraightSignService m_denmSigner;

            vanetza::security::v3::DummySignService m_camFallback;
        };

} // namespace ns3
