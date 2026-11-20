#include "denm_v3_sign_service.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"


#include <boost/variant/get.hpp>

#include <filesystem>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/security/v3/persistence.hpp>
#include <vanetza/security/v3/issuer_memory_lookup.hpp>
#include <vanetza/security/v3/trust_store.hpp>
#include <vanetza/security/v3/secured_message.hpp>

#include <ns3/log.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ns3 {

namespace
{
std::filesystem::path
RequireVehiclePkiFile(
    const std::filesystem::path& directory,
    const char* filename)
{
    const auto path = directory / filename;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec)
    {
        throw std::runtime_error(
            "vehicle CAM/DENM PKI configuration is incomplete");
    }
    return path;
}
} // namespace

    DenmV3SignService::DenmV3SignService(
        vanetza::ManualRuntime& runtime,
        vanetza::PositionProvider& position,
        const ActiveCredentialProvider& credentialProvider,
        CredentialMaterialResolver& materialResolver)
    :
        m_backend(
            vanetza::security::create_backend("default")),

        m_certificateProvider(nullptr),
        m_signHeaderPolicy(nullptr),
        m_locationChecker(),
        m_issuerLookup(),
        m_trustStore(),
        m_certificateValidator(),
        m_denmSigner(nullptr),

        m_credentialProvider(credentialProvider),
        m_materialResolver(materialResolver),

        m_runtime(runtime),
        m_position(position)
    {
        if (!m_backend)
        {
            throw std::runtime_error(
                "Unable to create Vanetza crypto backend");
        }

        // Validate certificate time and geographic constraints against the
        // same deterministic runtime and WGS84 position used to sign packets.
        m_certificateValidator.use_runtime(&m_runtime);
        m_certificateValidator.use_position_provider(&m_position);
        m_certificateValidator.use_location_checker(&m_locationChecker);

        const VehicleCredential* activeCredential =
            m_credentialProvider.GetActiveCredential();
        if (activeCredential == nullptr)
        {
            throw std::runtime_error(
                "vehicle CAM/DENM V3 signer has no active ticket");
        }

        const std::filesystem::path pkiDirectory =
            std::filesystem::path(activeCredential->certFile).parent_path();
        const auto rootPath =
            RequireVehiclePkiFile(pkiDirectory, "denm-root.cert");
        const auto aaPath =
            RequireVehiclePkiFile(pkiDirectory, "denm-aa.cert");

        const auto root =
            vanetza::security::v3::load_certificate_from_file(
                rootPath.string());
        const auto aa =
            vanetza::security::v3::load_certificate_from_file(
                aaPath.string());

        if (!m_issuerLookup.insert(aa) ||
            !m_issuerLookup.insert(root))
        {
            throw std::runtime_error(
                "vehicle CAM/DENM PKI issuer configuration is invalid");
        }

        // The root is the only trust anchor; AA is an issuer, not an
        // independently trusted certificate.
        m_trustStore.insert(root);
        m_certificateValidator.use_issuer_lookup(&m_issuerLookup);
        m_certificateValidator.use_trust_store(&m_trustStore);

        if (!ReloadActiveCredential())
        {
            throw std::runtime_error(
                "Unable to initialize vehicle V3 signer credential");
        }
    }



    bool
    DenmV3SignService::ReloadActiveCredential()
    {
        const VehicleCredential* credential =
            m_credentialProvider.GetActiveCredential();

        if (credential == nullptr)
        {
            return false;
        }

        std::unique_ptr<DenmV3CredentialMaterial> material;

        try
        {
            material =
                m_materialResolver.Resolve(*credential);
        }
        catch (...)
        {
            return false;
        }

        if (!material)
        {
            return false;
        }

        /*
         * P11 rotation policy:
         * a credential candidate must already be valid for DENM signing
         * at the current simulation time before it can replace the
         * currently active signer.
         *
         * This keeps temporal/policy failures at rotation time instead
         * of allowing an unusable signer to become active.
         */
        // A vehicle ticket can authorize CAM (AID 36), DENM (AID 37), or
        // both. StraightSignService still checks the requested AID on every
        // transmission; initialization rejects tickets authorized for neither.
        const auto camVerdict =
            m_certificateValidator.valid_for_signing(
                material->certificate,
                static_cast<vanetza::ItsAid>(36));
        const auto denmVerdict =
            m_certificateValidator.valid_for_signing(
                material->certificate,
                vanetza::aid::DEN);
        using Verdict =
            vanetza::security::v3::CertificateValidator::Verdict;
        if (camVerdict != Verdict::Valid &&
            denmVerdict != Verdict::Valid)
        {
            return false;
        }

        try
        {
            auto newCertificateProvider =
                std::make_unique<
                    vanetza::security::v3::
                        StaticCertificateProvider>(
                    material->certificate,
                    material->privateKey);

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
            // currently active signer.
            m_certificateProvider =
                std::move(newCertificateProvider);

            m_signHeaderPolicy =
                std::move(newSignHeaderPolicy);

            m_denmSigner =
                std::move(newDenmSigner);

            return true;
        }
        catch (...)
        {
            return false;
        }
    }


    vanetza::security::SignConfirm
    DenmV3SignService::sign(
        vanetza::security::SignRequest&& request)
    {
        if (request.its_aid == vanetza::aid::DEN ||
            request.its_aid == static_cast<vanetza::ItsAid>(36))
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
                    "vehicle CAM/DENM V3 signing");
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
                        "[V2X_V3_SIGNER_TX] "
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
                            "vehicle CAM/DENM V3 signed "
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
                        "[V2X_V3_SIGN] "
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
                        "[V2X_V3_SIGN] ERROR unexpected_non_v3_secured_message");
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
                        "vehicle V3 sign_failed error=" +
                        std::to_string(
                            static_cast<int>(confirm.error)));
                }

                NS_LOG_UNCOND(
                    "[V2X_V3_SIGN] ERROR sign_failed error="
                    << static_cast<int>(confirm.error));
            }

            return confirm;
        }

        // Unsupported AIDs are never silently downgraded to a dummy
        // signature. The caller receives an explicit signing failure.
        NS_LOG_UNCOND(
            "[V2X_V3_SIGN] ERROR unsupported_its_aid="
            << request.its_aid);
        return vanetza::security::SignConfirm::failure(
            vanetza::security::SignConfirmError::No_Certificate);
    }

} // namespace ns3

