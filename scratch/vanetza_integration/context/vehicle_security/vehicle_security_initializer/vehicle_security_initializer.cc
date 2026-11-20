#include "vehicle_security_initializer.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

namespace
{
std::string g_denmPkiDirectory;

struct CredentialCandidate
{
    uint64_t sequence = 0;
    VehicleCredential credential;
};

bool
IsRegularFile(
    const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && !ec;
}

} // namespace

void
VehicleSecurityInitializer::SetDenmPkiDirectory(
    const std::string& directory)
{
    g_denmPkiDirectory = directory;
}

std::vector<VehicleCredential>
VehicleSecurityInitializer::DiscoverDenmCredentials(
    const std::string& directory,
    const std::string& vehId)
{
    namespace fs = std::filesystem;

    std::vector<CredentialCandidate> candidates;

    const fs::path root(directory);

    std::error_code ec;

    if (!fs::is_directory(root, ec) || ec)
    {
        return {};
    }

    const std::string prefix =
        vehId + "-ticket-";

    const std::regex sequencePattern(
        "^" + prefix + "([0-9]+)\\.cert$");

    for (const auto& entry : fs::directory_iterator(root, ec))
    {
        if (ec)
        {
            break;
        }

        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string filename =
            entry.path().filename().string();

        std::smatch match;

        if (!std::regex_match(
                filename,
                match,
                sequencePattern))
        {
            continue;
        }

        const uint64_t sequence =
            std::stoull(match[1].str());

        const fs::path certPath =
            entry.path();

        const fs::path keyPath =
            root / (
                prefix +
                match[1].str() +
                ".pem");

        if (!IsRegularFile(keyPath))
        {
            continue;
        }

        candidates.push_back(
            {
                sequence,
                VehicleCredential{
                    vehId + "-ticket-" + match[1].str(),
                    vehId,
                    sequence,
                    certPath.string(),
                    keyPath.string()
                }
            });
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const CredentialCandidate& lhs,
           const CredentialCandidate& rhs)
        {
            if (lhs.sequence != rhs.sequence)
            {
                return lhs.sequence < rhs.sequence;
            }

            return lhs.credential.certFile <
                   rhs.credential.certFile;
        });

    if (!candidates.empty())
    {
        std::vector<VehicleCredential> credentials;

        credentials.reserve(candidates.size());

        for (const auto& candidate : candidates)
        {
            credentials.push_back(
                candidate.credential);
        }

        return credentials;
    }

    // Backward-compatible single-ticket layout.
    const fs::path legacyCert =
        root / (vehId + "-ticket.cert");

    const fs::path legacyKey =
        root / (vehId + "-ticket.pem");

    if (IsRegularFile(legacyCert) &&
        IsRegularFile(legacyKey))
    {
        return {
            VehicleCredential{
                vehId + "-ticket",
                vehId,
                0,
                legacyCert.string(),
                legacyKey.string()
            }
        };
    }

    return {};
}

void
VehicleSecurityInitializer::Initialize(
    VehicleState& state)
{
    // Reset the selected ticket and its OpenSSL compatibility material.
    state.security.credentialPool.SetCredentials({});
    if (state.security.privateKey != nullptr)
    {
        EVP_PKEY_free(state.security.privateKey);
        state.security.privateKey = nullptr;
    }
    if (state.security.certificate != nullptr)
    {
        X509_free(state.security.certificate);
        state.security.certificate = nullptr;
    }
    state.security.certFile.clear();
    state.security.keyFile.clear();

    // ----------------------------------------------------------------------
    // Real ETSI ITS Security V3 credential paths.
    //
    // The active VehicleCredential is the common ETSI V3 credential source
    // for CAM and DENM. Missing PKI material is not replaced by an ad-hoc
    // self-signed certificate.
    // ----------------------------------------------------------------------
    std::string pkiDir =
        g_denmPkiDirectory;

    if (pkiDir.empty())
    {
        const char* envPkiDir =
            std::getenv("V2X_DENM_PKI_DIR");

        if (envPkiDir != nullptr &&
            *envPkiDir != '\0')
        {
            pkiDir = envPkiDir;
        }
    }

    if (!pkiDir.empty())
    {
        const auto credentials =
            DiscoverDenmCredentials(
                pkiDir,
                state.identity.vehId);

        if (!credentials.empty())
        {
            state.security.credentialPool.SetCredentials(
                credentials);

            if (state.security.SetActiveDenmCredential(0))
            {
                return;
            }

            state.security.credentialPool.SetCredentials({});
        }

    }

    // No self-signed credential is synthesized. The GeoNetworking runtime
    // fails initialization unless a real V3 ticket/key pair was discovered
    // and validated above.
}

} // namespace ns3
