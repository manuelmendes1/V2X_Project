#include "denm_v3_sign_service.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"


#include <boost/variant/get.hpp>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/security/v3/persistence.hpp>
#include <vanetza/security/v3/secured_message.hpp>

#include <ns3/log.h>

#include <fstream>
#include <iomanip>
#include <sstream>
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
        const VehicleCredential& credential)
    :
        m_backend(
            vanetza::security::create_backend("default")),

        m_certificateProvider(nullptr),
        m_signHeaderPolicy(nullptr),
        m_certificateValidator(),
        m_denmSigner(nullptr),
        m_camFallback(runtime),
        m_runtime(runtime),
        m_position(position)
    {
        if (!m_backend)
        {
            throw std::runtime_error(
                "Unable to create Vanetza crypto backend");
        }

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

        if (!SetActiveCredential(credential))
        {
            throw std::runtime_error(
                "Unable to initialize DENM V3 signer credential");
        }
    }


    bool
    DenmV3SignService::SetActiveCredential(
        const VehicleCredential& credential)
    {
        if (credential.certFile.empty() ||
            credential.keyFile.empty())
        {
            return false;
        }

        try
        {
            RequireFile(
                credential.certFile,
                "certificate file");

            RequireFile(
                credential.keyFile,
                "private key file");

            auto newCertificateProvider =
                std::make_unique<
                    vanetza::security::v3::
                        StaticCertificateProvider>(
                    vanetza::security::v3::
                        load_certificate_from_file(
                            credential.certFile),
                    LoadPrivateKey(
                        credential.keyFile));

            auto newSignHeaderPolicy =
                std::make_unique<
                    vanetza::security::v3::
                        DefaultSignHeaderPolicy>(
                    m_runtime,
                    m_position,
                    *newCertificateProvider);

            auto newDenmSigner =
                std::make_unique<
                    vanetza::security::v3::
                        StraightSignService>(
                    *newCertificateProvider,
                    *m_backend,
                    *newSignHeaderPolicy,
                    m_certificateValidator);

            // All replacement objects are valid before touching the
            // currently active signer. This leaves the old credential
            // intact if loading or construction fails.
            m_certificateProvider =
                std::move(newCertificateProvider);

            m_signHeaderPolicy =
                std::move(newSignHeaderPolicy);

            m_denmSigner =
                std::move(newDenmSigner);

            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    vanetza::security::SignConfirm
    DenmV3SignService::sign(
        vanetza::security::SignRequest&& request)
    {
        if (request.its_aid == vanetza::aid::DEN)
        {
            // -----------------------------------------------------------------
            // Logical SIGN stage for the common V2X taxonomy.
            //
            // DENM signing is deliberately delegated to the V3 security
            // service after the application ENCODE stage. The deferred
            // marker remains in denm_service.cc; here we emit the actual
            // structured SIGN lifecycle event consumed by v2x_analyzer.
            // -----------------------------------------------------------------
            auto txContext = TxBridgeContext::Current();

            if (txContext)
            {
                PacketContext logCtx = *txContext;
                logCtx.pipelineStage = PipelineStage::SIGN;
                logCtx.pipelineStep = 3;
                logCtx.pipelineTotal = 8;
                logCtx.pipelineResult = PipelineResult::BEGIN;

                V2XWriteLog(
                    logCtx,
                    V2XLogLevel::DEBUG,
                    "SECURITY",
                    "SIGN_BEGIN",
                    "DENM_V3 signing");
            }

            auto confirm = m_denmSigner->sign(std::move(request));

            if (confirm.secured_message)
            {
                const auto* v3Message =
                    boost::get<vanetza::security::v3::SecuredMessage>(
                        &*confirm.secured_message);

                if (v3Message != nullptr)
                {
                    const auto signerIdentifier =
                        v3Message->signer_identifier();

                    const bool hasCertificate =
                        vanetza::security::v3::contains_certificate(
                            signerIdentifier);

                    const auto certificateId =
                        vanetza::security::v3::get_certificate_id(
                            signerIdentifier);

                    std::string certificateIdHex;

                    if (certificateId)
                    {
                        std::ostringstream certificateIdStream;

                        for (const auto byte : *certificateId)
                        {
                            certificateIdStream
                                << std::hex
                                << std::setw(2)
                                << std::setfill('0')
                                << static_cast<unsigned int>(
                                       static_cast<unsigned char>(byte));
                        }

                        certificateIdHex = certificateIdStream.str();
                    }

                    if (txContext && !certificateIdHex.empty())
                    {
                        txContext->signerCertificateIdHex =
                            certificateIdHex;
                    }

                    NS_LOG_UNCOND(
                        "[DENM_V3_SIGNER_TX] "
                        "contains_certificate="
                        << (hasCertificate ? 1 : 0)
                        << " "
                        "certificate_id="
                        << (certificateId ? 1 : 0));

                    if (txContext)
                    {
                        PacketContext logCtx = *txContext;
                        logCtx.pipelineStage = PipelineStage::SIGN;
                        logCtx.pipelineStep = 3;
                        logCtx.pipelineTotal = 8;
                        logCtx.pipelineResult = PipelineResult::OK;

                        V2XWriteLog(
                            logCtx,
                            V2XLogLevel::DEBUG,
                            "SECURITY",
                            "SIGN_OK",
                            "DENM_V3 signed "
                            "protocol_version=" +
                            std::to_string(
                                static_cast<unsigned>(
                                    v3Message->protocol_version())) +
                            " aid=" +
                            std::to_string(v3Message->its_aid()) +
                            " signed=" +
                            std::to_string(
                                v3Message->is_signed() ? 1 : 0));
                    }

                    NS_LOG_UNCOND(
                        "[DENM_V3_SIGN] "
                        "protocol_version=" << static_cast<unsigned>(
                            v3Message->protocol_version())
                        << " aid=" << v3Message->its_aid()
                        << " signed=" << (v3Message->is_signed() ? 1 : 0));
                }
                else
                {
                    if (txContext)
                    {
                        PacketContext logCtx = *txContext;
                        logCtx.pipelineStage = PipelineStage::SIGN;
                        logCtx.pipelineStep = 3;
                        logCtx.pipelineTotal = 8;
                        logCtx.pipelineResult = PipelineResult::FAIL;

                        V2XWriteLog(
                            logCtx,
                            V2XLogLevel::ERROR,
                            "SECURITY",
                            "SIGN_FAIL",
                            "DENM_V3 unexpected_non_v3_secured_message");
                    }

                    NS_LOG_UNCOND(
                        "[DENM_V3_SIGN] ERROR unexpected_non_v3_secured_message");
                }
            }
            else
            {
                if (txContext)
                {
                    PacketContext logCtx = *txContext;
                    logCtx.pipelineStage = PipelineStage::SIGN;
                    logCtx.pipelineStep = 3;
                    logCtx.pipelineTotal = 8;
                    logCtx.pipelineResult = PipelineResult::FAIL;

                    V2XWriteLog(
                        logCtx,
                        V2XLogLevel::ERROR,
                        "SECURITY",
                        "SIGN_FAIL",
                        "DENM_V3 sign_failed error=" +
                        std::to_string(
                            static_cast<int>(confirm.error)));
                }

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

