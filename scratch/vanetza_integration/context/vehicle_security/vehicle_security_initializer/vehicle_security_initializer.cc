#include "vehicle_security_initializer.h"

#include "vanetza_integration/security/vehicle_crypto/vehicle_crypto.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_set>
#include <fstream>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ns3
{

namespace
{
std::string g_denmPkiDirectory;

struct ResolvedVehicleCredentialMapping
{
    VehicleCredentialMapping mapping;
    std::string resolvedDirectory;
};

struct VehiclePkiConfiguration
{
    bool enabled = false;
    std::string runDirectory;
    std::vector<ResolvedVehicleCredentialMapping> mappings;
};

// Written during startup and read-only after SimulationController::Start().
VehiclePkiConfiguration g_vehiclePkiConfiguration;

bool
IsRegularFile(
    const std::filesystem::path& path)
{
    // symlink_status() inspects the path itself without following
    // a symbolic link to its target.
    std::error_code ec;
    const auto status =
        std::filesystem::symlink_status(path, ec);

    return !ec && std::filesystem::is_regular_file(status);
}

std::string
TrimWhitespace(const std::string& value)
{
    const auto first =
        value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
    {
        return {};
    }

    const auto last =
        value.find_last_not_of(" \t\r\n");

    return value.substr(first, last - first + 1);
}

bool
IsSafeRelativeCredentialDirectory(const std::string& value)
{
    if (value.empty() ||
        value.front() == '/' ||
        value.back() == '/' ||
        value.find('\\') != std::string::npos ||
        value.find("//") != std::string::npos ||
        value.find(':') != std::string::npos)
    {
        return false;
    }

    std::size_t start = 0;

    while (start < value.size())
    {
        const std::size_t separator = value.find('/', start);

        const std::string component =
            separator == std::string::npos
                ? value.substr(start)
                : value.substr(start, separator - start);

        if (component.empty() ||
            component == "." ||
            component == "..")
        {
            return false;
        }

        if (separator == std::string::npos)
        {
            break;
        }

        start = separator + 1;
    }

    return true;
}

bool
IsSafeCredentialFilePrefix(const std::string& value)
{
    if (value.empty() || value == "." || value == "..")
    {
        return false;
    }

    for (const unsigned char character : value)
    {
        const bool alphanumeric =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9');

        const bool allowedPunctuation =
            character == '_' ||
            character == '-' ||
            character == '.';

        if (!alphanumeric && !allowedPunctuation)
        {
            return false;
        }
    }

    return true;
}

} // namespace

void
VehicleSecurityInitializer::SetDenmPkiDirectory(
    const std::string& directory)
{
    g_denmPkiDirectory = directory;
}

bool
VehicleSecurityInitializer::ConfigureVehiclePki(
    const std::string& runDirectory,
    const std::string& mappingFile,
    std::string& error)
{
    namespace fs = std::filesystem;

    error.clear();

    // An entirely empty pair explicitly disables strict vehicle PKI mode.
    if (runDirectory.empty() && mappingFile.empty())
    {
        g_vehiclePkiConfiguration = VehiclePkiConfiguration{};
        return true;
    }

    // The two configuration paths form one atomic configuration unit.
    if (runDirectory.empty() || mappingFile.empty())
    {
        error =
            "vehicle PKI requires both run directory and mapping file";
        return false;
    }

    std::error_code ec;

    const fs::path canonicalRunDirectory =
        fs::canonical(fs::path(runDirectory), ec);

    if (ec)
    {
        error =
            "cannot resolve vehicle PKI run directory: " + ec.message();
        return false;
    }

    ec.clear();

    if (!fs::is_directory(canonicalRunDirectory, ec) || ec)
    {
        error = "vehicle PKI run path is not a directory";
        return false;
    }

    // Reject a mapping file that is itself a symlink.
    if (!IsRegularFile(fs::path(mappingFile)))
    {
        error =
            "vehicle credential mapping must be a regular non-symlink file";
        return false;
    }

    ec.clear();

    const fs::path canonicalMappingFile =
        fs::canonical(fs::path(mappingFile), ec);

    if (ec)
    {
        error =
            "cannot resolve vehicle credential mapping file: " +
            ec.message();
        return false;
    }

    std::vector<VehicleCredentialMapping> parsedMappings;

    if (!LoadVehicleCredentialMappings(
            canonicalMappingFile.string(),
            parsedMappings,
            error))
    {
        return false;
    }

    // Build a candidate configuration locally. The active configuration
    // is not modified until every mapping has passed path validation.
    VehiclePkiConfiguration candidate;
    candidate.runDirectory = canonicalRunDirectory.string();
    candidate.mappings.reserve(parsedMappings.size());

    // A credential selector (canonical directory + filename prefix)
    // must belong to exactly one SUMO vehicle.
    std::unordered_set<std::string> assignedCredentialSelectors;

    for (const auto& mapping : parsedMappings)
    {
        std::string resolvedDirectory;
        std::string resolutionError;

        if (!ResolveVehicleCredentialDirectory(
                candidate.runDirectory,
                mapping,
                resolvedDirectory,
                resolutionError))
        {
            error =
                "SUMO vehicle '" + mapping.sumoVehicleId + "': " +
                resolutionError;
            return false;
        }

        std::string credentialSelector = resolvedDirectory;
        credentialSelector.push_back('\0');
        credentialSelector += mapping.credentialFilePrefix;

        if (!assignedCredentialSelectors.insert(credentialSelector).second)
        {
            error =
                "SUMO vehicle '" + mapping.sumoVehicleId +
                "': credential directory and prefix already assigned";
            return false;
        }

        candidate.mappings.push_back(
            ResolvedVehicleCredentialMapping{
                mapping,
                resolvedDirectory
            });
    }

    if (candidate.mappings.empty())
    {
        error = "vehicle credential mapping contains no usable entries";
        return false;
    }

    candidate.enabled = true;

    // Atomic activation: this is the only mutation of active configuration.
    g_vehiclePkiConfiguration = std::move(candidate);

    error.clear();
    return true;
}

bool
VehicleSecurityInitializer::LoadVehicleCredentialMappings(
    const std::string& mappingFile,
    std::vector<VehicleCredentialMapping>& mappings,
    std::string& error)
{
    namespace fs = std::filesystem;

    // The caller must never observe a partially loaded mapping.
    mappings.clear();
    error.clear();

    std::ifstream input(mappingFile);

    if (!input.is_open())
    {
        error = mappingFile + ": cannot open mapping file";
        return false;
    }

    std::vector<VehicleCredentialMapping> parsed;
    std::unordered_set<std::string> seenVehicleIds;

    std::string line;
    std::size_t lineNumber = 0;

    const auto fail =
        [&](const std::string& reason) -> bool
        {
            error = mappingFile;

            if (lineNumber > 0)
            {
                error += ":" + std::to_string(lineNumber);
            }

            error += ": " + reason;
            mappings.clear();
            return false;
        };

    while (std::getline(input, line))
    {
        ++lineNumber;

        // Accept a UTF-8 BOM at the beginning of the file.
        if (lineNumber == 1 &&
            line.compare(0, 3, "\xEF\xBB\xBF") == 0)
        {
            line.erase(0, 3);
        }

        const std::string trimmedLine =
            TrimWhitespace(line);

        // Blank lines and full-line comments are ignored.
        if (trimmedLine.empty() || trimmedLine.front() == '#')
        {
            continue;
        }

        const std::size_t firstTab = line.find('\t');

        if (firstTab == std::string::npos)
        {
            return fail("expected exactly three TSV columns");
        }

        const std::size_t secondTab =
            line.find('\t', firstTab + 1);

        if (secondTab == std::string::npos ||
            line.find('\t', secondTab + 1) != std::string::npos)
        {
            return fail("expected exactly three TSV columns");
        }

        const std::string sumoVehicleId =
            TrimWhitespace(line.substr(0, firstTab));

        const std::string credentialDirectoryRelative =
            TrimWhitespace(
                line.substr(
                    firstTab + 1,
                    secondTab - firstTab - 1));

        const std::string credentialFilePrefix =
            TrimWhitespace(line.substr(secondTab + 1));

        if (sumoVehicleId.empty() ||
            credentialDirectoryRelative.empty() ||
            credentialFilePrefix.empty())
        {
            return fail("mapping fields must not be empty");
        }

        if (!IsSafeRelativeCredentialDirectory(
                credentialDirectoryRelative))
        {
            return fail(
                "credential directory must be a safe relative path");
        }

        if (!IsSafeCredentialFilePrefix(credentialFilePrefix))
        {
            return fail(
                "credential file prefix contains unsafe characters");
        }

        if (!seenVehicleIds.insert(sumoVehicleId).second)
        {
            return fail(
                "duplicate SUMO vehicle ID: " + sumoVehicleId);
        }

        parsed.push_back(
            VehicleCredentialMapping{
                sumoVehicleId,
                credentialDirectoryRelative,
                credentialFilePrefix
            });
    }

    if (input.bad())
    {
        return fail("I/O error while reading mapping file");
    }

    if (parsed.empty())
    {
        return fail("mapping file contains no entries");
    }

    mappings.swap(parsed);
    error.clear();

    return true;
}



bool
VehicleSecurityInitializer::ResolveVehicleCredentialDirectory(
    const std::string& pkiRunDirectory,
    const VehicleCredentialMapping& mapping,
    std::string& resolvedDirectory,
    std::string& error)
{
    namespace fs = std::filesystem;

    resolvedDirectory.clear();
    error.clear();

    const auto fail =
        [&](const std::string& reason) -> bool
        {
            resolvedDirectory.clear();
            error = reason;
            return false;
        };

    if (pkiRunDirectory.empty())
    {
        return fail("PKI run directory is empty");
    }

    if (mapping.sumoVehicleId.empty())
    {
        return fail("SUMO vehicle ID is empty");
    }

    // Revalidate the mapping because this public method may be called
    // without first passing through LoadVehicleCredentialMappings().
    if (!IsSafeRelativeCredentialDirectory(
            mapping.credentialDirectoryRelative))
    {
        return fail("credential directory is not a safe relative path");
    }

    if (!IsSafeCredentialFilePrefix(mapping.credentialFilePrefix))
    {
        return fail("credential file prefix is invalid");
    }

    std::error_code ec;

    // canonical() resolves symlinks and produces an absolute path.
    const fs::path canonicalRoot =
        fs::canonical(fs::path(pkiRunDirectory), ec);

    if (ec)
    {
        return fail(
            "cannot resolve PKI run directory: " + ec.message());
    }

    ec.clear();

    if (!fs::is_directory(canonicalRoot, ec) || ec)
    {
        return fail("PKI run path is not a directory");
    }

    const fs::path requestedDirectory =
        canonicalRoot /
        fs::path(mapping.credentialDirectoryRelative);

    ec.clear();

    const fs::path canonicalCredentialDirectory =
        fs::canonical(requestedDirectory, ec);

    if (ec)
    {
        return fail(
            "cannot resolve credential directory: " + ec.message());
    }

    ec.clear();

    if (!fs::is_directory(canonicalCredentialDirectory, ec) || ec)
    {
        return fail("credential path is not a directory");
    }

    // Compare filesystem path components, not string prefixes.
    // This also rejects symlinks that resolve outside the PKI run.
    auto rootComponent = canonicalRoot.begin();
    auto directoryComponent = canonicalCredentialDirectory.begin();

    while (rootComponent != canonicalRoot.end() &&
           directoryComponent != canonicalCredentialDirectory.end() &&
           *rootComponent == *directoryComponent)
    {
        ++rootComponent;
        ++directoryComponent;
    }

    const bool isStrictDescendant =
        rootComponent == canonicalRoot.end() &&
        directoryComponent != canonicalCredentialDirectory.end();

    if (!isStrictDescendant)
    {
        return fail(
            "credential directory escapes the PKI run directory");
    }

    resolvedDirectory = canonicalCredentialDirectory.string();
    error.clear();

    return true;
}

std::vector<VehicleCredential>
VehicleSecurityInitializer::DiscoverVehicleCredentials(
    const std::string& directory,
    const std::string& sumoVehicleId,
    const std::string& credentialFilePrefix)
{
    namespace fs = std::filesystem;

    if (sumoVehicleId.empty() || credentialFilePrefix.empty())
    {
        return {};
    }

    const fs::path root(directory);
    std::error_code ec;

    if (!fs::is_directory(root, ec) || ec)
    {
        return {};
    }

    struct CredentialCandidate
    {
        uint64_t sequence = 0;
        VehicleCredential credential;
    };

    std::vector<CredentialCandidate> candidates;

    const std::string filenamePrefix =
        credentialFilePrefix + "-";

    constexpr std::string_view certificateSuffix = ".cert";

    fs::directory_iterator iterator(root, ec);
    const fs::directory_iterator end;

    for (; !ec && iterator != end; iterator.increment(ec))
    {
        const auto& entry = *iterator;

        // Do not follow certificate symlinks outside this directory.
        if (!IsRegularFile(entry.path()))
        {
            continue;
        }

        const std::string filename =
            entry.path().filename().string();

        if (!filename.starts_with(filenamePrefix) ||
            !filename.ends_with(certificateSuffix))
        {
            continue;
        }

        const std::size_t sequenceStart =
            filenamePrefix.size();

        if (filename.size() <=
            filenamePrefix.size() + certificateSuffix.size())
        {
            continue;
        }

        const std::size_t sequenceLength =
            filename.size() -
            sequenceStart -
            certificateSuffix.size();

        if (sequenceLength == 0)
        {
            continue;
        }

        const std::string sequenceText =
            filename.substr(sequenceStart, sequenceLength);

        const bool numeric =
            std::all_of(
                sequenceText.begin(),
                sequenceText.end(),
                [](unsigned char value)
                {
                    return value >= '0' && value <= '9';
                });

        if (!numeric)
        {
            continue;
        }

        uint64_t sequence = 0;

        try
        {
            std::size_t consumed = 0;
            sequence = std::stoull(sequenceText, &consumed);

            if (consumed != sequenceText.size())
            {
                continue;
            }
        }
        catch (...)
        {
            continue;
        }

        const fs::path certificatePath = entry.path();

        const fs::path keyPath =
            root / (
                credentialFilePrefix + "-" +
                sequenceText + ".pem");

        if (!IsRegularFile(keyPath))
        {
            continue;
        }

        candidates.push_back(
            {
                sequence,
                VehicleCredential{
                    credentialFilePrefix + "-" + sequenceText,
                    sumoVehicleId,
                    sequence,
                    certificatePath.string(),
                    keyPath.string()
                }
            });
    }

    // Do not publish a partial inventory if directory iteration failed.
    if (ec)
    {
        return {};
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

            return lhs.credential.credentialId <
                   rhs.credential.credentialId;
        });

    std::vector<VehicleCredential> credentials;
    credentials.reserve(candidates.size());

    for (const auto& candidate : candidates)
    {
        credentials.push_back(candidate.credential);
    }

    return credentials;
}


std::vector<VehicleCredential>
VehicleSecurityInitializer::DiscoverDenmCredentials(
    const std::string& directory,
    const std::string& vehId)
{
    if (vehId.empty())
    {
        return {};
    }

    auto credentials =
        DiscoverVehicleCredentials(
            directory,
            vehId,
            vehId + "-ticket");

    if (!credentials.empty())
    {
        return credentials;
    }

    // Preserve the older single-ticket layout.
    namespace fs = std::filesystem;

    const fs::path root(directory);
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

bool
VehicleSecurityInitializer::Initialize(
    VehicleState& state,
    std::string& error)
{
    namespace fs = std::filesystem;

    error.clear();

    // Reset the credential selection and any previous security material.
    // VehicleSecurityContext is the owner of these OpenSSL objects.
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
    state.security.certLoaded = false;
    state.security.vehiclePkiRunDirectory.clear();

    // ----------------------------------------------------------------------
    // Explicit vehicle PKI mode.
    //
    // This mode is strict and must never downgrade to locally generated
    // cryptographic material.
    // ----------------------------------------------------------------------
    if (g_vehiclePkiConfiguration.enabled)
    {
        const auto mapping = std::find_if(
            g_vehiclePkiConfiguration.mappings.begin(),
            g_vehiclePkiConfiguration.mappings.end(),
            [&](const ResolvedVehicleCredentialMapping& candidate)
            {
                return candidate.mapping.sumoVehicleId ==
                       state.identity.vehId;
            });

        if (mapping == g_vehiclePkiConfiguration.mappings.end())
        {
            error =
                "no mapping for SUMO vehicle ID '" +
                state.identity.vehId + "'";
            return false;
        }

        const auto credentials =
            DiscoverVehicleCredentials(
                mapping->resolvedDirectory,
                state.identity.vehId,
                mapping->mapping.credentialFilePrefix);

        if (credentials.empty())
        {
            error =
                "no credential files for SUMO vehicle ID '" +
                state.identity.vehId + "'";
            return false;
        }

        // Keep the complete ordered inventory. Each candidate is activated
        // only after certificate/private-key binding validation succeeds.
        state.security.credentialPool.SetCredentials(credentials);

        for (std::size_t index = 0;
             index < credentials.size();
             ++index)
        {
            bool activated = false;

            try
            {
                activated =
                    state.security.SetActiveDenmCredential(index);
            }
            catch (...)
            {
                activated = false;
            }

            if (activated)
            {
                // Associate the canonical issuance run only after a
                // credential has passed binding validation and activation.
                state.security.vehiclePkiRunDirectory =
                    g_vehiclePkiConfiguration.runDirectory;

                error.clear();
                return true;
            }
        }

        state.security.credentialPool.SetCredentials({});

        error =
            "no credential could be activated for SUMO vehicle ID '" +
            state.identity.vehId + "'";

        return false;
    }

    // ----------------------------------------------------------------------
    // Legacy compatibility mode.
    //
    // This path is used only when explicit vehicle PKI configuration
    // has not been activated.
    // ----------------------------------------------------------------------
    std::string pkiDir = g_denmPkiDirectory;

    if (pkiDir.empty())
    {
        const char* envPkiDir =
            std::getenv("V2X_DENM_PKI_DIR");

        if (envPkiDir != nullptr && *envPkiDir != '\0')
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
                return true;
            }

            state.security.credentialPool.SetCredentials({});
        }

        // Preserve the previous legacy path contract.
        state.security.certFile =
            pkiDir + "/" +
            state.identity.vehId +
            "-ticket.cert";

        state.security.keyFile =
            pkiDir + "/" +
            state.identity.vehId +
            "-ticket.pem";
    }

    // Historical local CAM material generation remains confined to
    // legacy mode. It is unreachable from explicit vehicle PKI mode.
    auto [key, cert] =
        GenerateEcdsaKeyAndCert(
            state.identity.vehId);

    state.security.privateKey = key;
    state.security.certificate = cert;

    if (key == nullptr || cert == nullptr)
    {
        error = "legacy security material generation failed";
        return false;
    }

    return true;
}

} // namespace ns3
