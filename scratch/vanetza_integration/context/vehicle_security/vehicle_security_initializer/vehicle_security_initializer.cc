#include "vehicle_security_initializer.h"

#include "vanetza_integration/security/vehicle_crypto/vehicle_crypto.h"

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
    DenmCredential credential;
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

std::vector<DenmCredential>
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
                {
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
        std::vector<DenmCredential> credentials;

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
            {
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
    // ----------------------------------------------------------------------
    // Legacy/local OpenSSL material.
    //
    // Mantido para não alterar o mecanismo CAM existente nesta fase.
    // ----------------------------------------------------------------------
    auto [key, cert] =
        GenerateEcdsaKeyAndCert(
            state.identity.vehId);

    state.security.privateKey = key;
    state.security.certificate = cert;

    // Reset the DENM credential selection state so initialization is
    // deterministic even if a VehicleState is initialized again.
    state.security.denmCredentials.clear();
    state.security.activeDenmCredentialIndex = 0;

    // ----------------------------------------------------------------------
    // Real ETSI ITS Security V3 credential paths for DENM.
    //
    // Explicit configuration has precedence over the environment variable.
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
            state.security.denmCredentials =
                credentials;

            if (state.security.SetActiveDenmCredential(0))
            {
                return;
            }

            state.security.denmCredentials.clear();
        }

        // Preserve the previous explicit path contract even when
        // discovery finds no usable credential pair.
        state.security.certFile =
            pkiDir + "/" +
            state.identity.vehId +
            "-ticket.cert";

        state.security.keyFile =
            pkiDir + "/" +
            state.identity.vehId +
            "-ticket.pem";
    }
}

} // namespace ns3
