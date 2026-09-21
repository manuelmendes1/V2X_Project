#include "denm_v3_sign_service.h"


#include <boost/variant/get.hpp>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/security/v3/persistence.hpp>
#include <vanetza/security/v3/secured_message.hpp>

#include <ns3/log.h>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace ns3 {

    namespace {

        vanetza::security::PrivateKey LoadPrivateKey(const std::string& path){

            const auto keyPair = vanetza::security::v3::load_private_key_from_file(path);

            vanetza::security::PrivateKey key;

            key.type = vanetza::security::KeyType::NistP256;

            key.key.assign(keyPair.private_key.key.begin(), keyPair.private_key.key.end());

            return key;
        }

        void RequireFile(const std::string& path, const char* description){

            std::ifstream input(path, std::ios::binary);

            if (!input.good()){

                throw std::runtime_error(std::string("DENM V3 ") + description + " not accessible: " + path);
            }
        }

    } // anonymous namespace


    DenmV3SignService::DenmV3SignService(
        vanetza::ManualRuntime& runtime,
        vanetza::PositionProvider& position,
        const std::string& certificateFile,
        const std::string& privateKeyFile)
    :
        m_backend(
            vanetza::security::create_backend("default")),

        m_certificateProvider(
            vanetza::security::v3::
                load_certificate_from_file(certificateFile),
            LoadPrivateKey(privateKeyFile)),

        m_signHeaderPolicy(
            runtime,
            position,
            m_certificateProvider),

        m_certificateValidator(),

        m_denmSigner(
            m_certificateProvider,
            *m_backend,
            m_signHeaderPolicy,
            m_certificateValidator),

        m_camFallback(runtime)
    {
        if (!m_backend)
        {
            throw std::runtime_error(
                "Unable to create Vanetza crypto backend");
        }

        RequireFile(
            certificateFile,
            "certificate file");

        RequireFile(
            privateKeyFile,
            "private key file");

        // ----------------------------------------------------------------------
        // Current ns-3 simulation time starts at zero, while the external
        // certificate has real-world validity dates.
        //
        // Do not disable this permanently. This is only the first integration
        // stage; simulation-time ↔ ITS certificate time mapping comes later.
        // ----------------------------------------------------------------------
        m_certificateValidator.disable_time_checks(true);

        // DENM location restriction is not used by this local ECC test PKI.
        m_certificateValidator.disable_location_checks(true);
    }


    vanetza::security::SignConfirm
    DenmV3SignService::sign(
        vanetza::security::SignRequest&& request)
    {
        if (request.its_aid == vanetza::aid::DEN)
        {
            auto confirm = m_denmSigner.sign(std::move(request));

            if (confirm.secured_message)
            {
                const auto* v3Message =
                    boost::get<vanetza::security::v3::SecuredMessage>(
                        &*confirm.secured_message);

                if (v3Message != nullptr)
                {
                    NS_LOG_UNCOND(
                        "[DENM_V3_SIGN] "
                        "protocol_version=" << static_cast<unsigned>(
                            v3Message->protocol_version())
                        << " aid=" << v3Message->its_aid()
                        << " signed=" << (v3Message->is_signed() ? 1 : 0));
                }
                else
                {
                    NS_LOG_UNCOND(
                        "[DENM_V3_SIGN] ERROR unexpected_non_v3_secured_message");
                }
            }
            else
            {
                NS_LOG_UNCOND(
                    "[DENM_V3_SIGN] ERROR sign_failed error="
                    << static_cast<int>(confirm.error));
            }

            return confirm;
        }

        // CAM and any other current application retain
        // the previous dummy behaviour.
        return m_camFallback.sign(
            std::move(request));
    }

} // namespace ns3

