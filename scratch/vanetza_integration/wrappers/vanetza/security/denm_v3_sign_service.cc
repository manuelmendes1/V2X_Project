#include "denm_v3_sign_service.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"


#include <boost/variant/get.hpp>

#include <vanetza/common/its_aid.hpp>
#include <vanetza/security/v3/persistence.hpp>
#include <vanetza/security/v3/secured_message.hpp>

#include <ns3/log.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <filesystem>

namespace ns3 {

namespace
{

constexpr vanetza::ItsAid kCamItsAid = 36;
constexpr vanetza::ItsAid kDenmItsAid = 37;

std::filesystem::path
RequireVehiclePkiRunDirectory(
    const std::string& directory)
{
    namespace fs = std::filesystem;

    if (directory.empty())
    {
        throw std::runtime_error(
            "Vehicle V3 PKI run directory is required");
    }

    std::error_code ec;

    const fs::path absoluteDirectory =
        fs::absolute(fs::path(directory), ec).lexically_normal();

    if (ec)
    {
        throw std::runtime_error(
            "Vehicle V3 PKI run directory cannot be resolved");
    }

    const fs::path canonicalDirectory =
        fs::canonical(absoluteDirectory, ec);

    if (ec || canonicalDirectory != absoluteDirectory)
    {
        throw std::runtime_error(
            "Vehicle V3 PKI run directory must be canonical "
            "and contain no symlink path components");
    }

    const auto status =
        fs::symlink_status(canonicalDirectory, ec);

    if (ec ||
        fs::is_symlink(status) ||
        !fs::is_directory(status))
    {
        throw std::runtime_error(
            "Vehicle V3 PKI run path is not a regular directory");
    }

    return canonicalDirectory;
}

std::filesystem::path
RequireVehiclePkiFile(
    const std::filesystem::path& directory,
    const char* relativeName)
{
    namespace fs = std::filesystem;

    const fs::path relativePath(relativeName);
    const fs::path parentPath =
        directory / relativePath.parent_path();

    std::error_code ec;

    const auto parentStatus =
        fs::symlink_status(parentPath, ec);

    if (ec ||
        fs::is_symlink(parentStatus) ||
        !fs::is_directory(parentStatus))
    {
        throw std::runtime_error(
            "Vehicle V3 PKI certificate directory is missing "
            "or is a symlink");
    }

    const fs::path filePath =
        directory / relativePath;

    ec.clear();

    const auto fileStatus =
        fs::symlink_status(filePath, ec);

    if (ec ||
        fs::is_symlink(fileStatus) ||
        !fs::is_regular_file(fileStatus))
    {
        throw std::runtime_error(
            "Vehicle V3 PKI certificate must be a regular "
            "non-symlink file: " + filePath.string());
    }

    ec.clear();

    const auto size =
        fs::file_size(filePath, ec);

    if (ec || size == 0)
    {
        throw std::runtime_error(
            "Vehicle V3 PKI certificate is empty or unreadable: " +
            filePath.string());
    }

    return filePath;
}

bool
SameCertificateEncoding(
    const vanetza::security::v3::Certificate& left,
    const vanetza::security::v3::Certificate& right)
{
    const auto leftEncoding = left.encode();
    const auto rightEncoding = right.encode();

    return leftEncoding.size() == rightEncoding.size() &&
           std::equal(
               leftEncoding.begin(),
               leftEncoding.end(),
               rightEncoding.begin());
}

} // namespace





    DenmV3SignService::DenmV3SignService(
        vanetza::ManualRuntime& runtime,
        vanetza::PositionProvider& position,
        const ActiveCredentialProvider& credentialProvider,
        CredentialMaterialResolver& materialResolver,
        const std::string& vehiclePkiRunDirectory)
    :
        m_backend(
            vanetza::security::create_backend("default")),

        m_certificateValidator(),
        m_certificateProvider(nullptr),
        m_signHeaderPolicy(nullptr),
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

        const auto directory =
            RequireVehiclePkiRunDirectory(vehiclePkiRunDirectory);

        const auto trustedRootPath =
            RequireVehiclePkiFile(
                directory,
                "trust-store/trusted-root.cert");

        const auto aaPath =
            RequireVehiclePkiFile(
                directory,
                "certificate-chain/aa.cert");

        const auto chainRootPath =
            RequireVehiclePkiFile(
                directory,
                "certificate-chain/root.cert");

        const auto trustedRoot =
            vanetza::security::v3::load_certificate_from_file(
                trustedRootPath.string());

        const auto aa =
            vanetza::security::v3::load_certificate_from_file(
                aaPath.string());

        const auto chainRoot =
            vanetza::security::v3::load_certificate_from_file(
                chainRootPath.string());

        // Only the configured Root is a trust anchor. The Root in the
        // issuer chain must be exactly the same OER certificate.
        if (!SameCertificateEncoding(trustedRoot, chainRoot))
        {
            throw std::runtime_error(
                "Vehicle V3 PKI trust Root does not match issuer-chain Root");
        }

        if (!m_issuerLookup.insert(aa) ||
            !m_issuerLookup.insert(chainRoot))
        {
            throw std::runtime_error(
                "Vehicle V3 PKI issuer chain is invalid");
        }

        m_trustStore.insert(trustedRoot);

        // The validator retains non-owning references to these objects.
        m_certificateValidator.use_runtime(&m_runtime);
        m_certificateValidator.use_position_provider(&m_position);
        m_certificateValidator.use_location_checker(&m_locationChecker);
        m_certificateValidator.use_issuer_lookup(&m_issuerLookup);
        m_certificateValidator.use_trust_store(&m_trustStore);

        if (!ReloadActiveCredential())
        {
            throw std::runtime_error(
                "Unable to initialize vehicle CAM/DENM V3 signer credential");
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

        // The same active ticket must explicitly authorize both CAM and
        // DENM. Do not activate a ticket that only covers one application.
        const auto camCredentialVerdict =
            m_certificateValidator.valid_for_signing(
                material->certificate,
                kCamItsAid);

        const auto denmCredentialVerdict =
            m_certificateValidator.valid_for_signing(
                material->certificate,
                kDenmItsAid);

        if (camCredentialVerdict !=
                vanetza::security::v3::CertificateValidator::Verdict::Valid ||
            denmCredentialVerdict !=
                vanetza::security::v3::CertificateValidator::Verdict::Valid)
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

            // The first message signed with this active ticket must carry
            // its full certificate so receivers can establish the signer.
            newSignHeaderPolicy->request_certificate();

            auto newDenmSigner =
                std::make_unique<
                    vanetza::security::v3::
                        StraightSignService>(
                    *newCertificateProvider,
                    *m_backend,
                    *newSignHeaderPolicy,
                    m_certificateValidator);

            // Destroy the old signer before replacing the objects it
            // references. The new objects remain alive after unique_ptr moves.
            m_denmSigner =
                std::move(newDenmSigner);

            m_signHeaderPolicy =
                std::move(newSignHeaderPolicy);

            m_certificateProvider =
                std::move(newCertificateProvider);

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
        if (request.its_aid != kCamItsAid &&
            request.its_aid != kDenmItsAid)
        {
            return vanetza::security::SignConfirm::failure(
                vanetza::security::SignConfirmError::No_Certificate);
        }

        if (!m_denmSigner)
        {
            return vanetza::security::SignConfirm::failure(
                vanetza::security::SignConfirmError::No_Certificate);
        }

        const std::string application =
            request.its_aid == kCamItsAid ? "CAM" : "DENM";

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
                application + "_V3 signing");
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
                    "[" << application << "_V3_SIGNER_TX] "
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
                        application + "_V3 signed "
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
                    "[" << application << "_V3_SIGN] "
                    "protocol_version="
                    << static_cast<unsigned>(
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
                        application + "_V3 unexpected_non_v3_secured_message");
                }

                NS_LOG_UNCOND(
                    "[" << application
                    << "_V3_SIGN] ERROR unexpected_non_v3_secured_message");
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
                    application + "_V3 sign_failed error=" +
                    std::to_string(
                        static_cast<int>(confirm.error)));
            }

            NS_LOG_UNCOND(
                "[" << application << "_V3_SIGN] ERROR sign_failed error="
                << static_cast<int>(confirm.error));
        }

        return confirm;
    }


} // namespace ns3

