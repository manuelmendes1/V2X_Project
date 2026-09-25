#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace ns3::denm_test
{

inline std::string
ResolveDenmPkiDirectory()
{
    namespace fs = std::filesystem;

    const char* env =
        std::getenv("DENM_TEST_PKI_DIR");

    if (env != nullptr &&
        *env != '\0')
    {
        const fs::path explicitPath(env);

        if (fs::is_directory(explicitPath))
        {
            return explicitPath.string();
        }

        return {};
    }

    const char* home =
        std::getenv("HOME");

    if (home == nullptr ||
        *home == '\0')
    {
        return {};
    }

    const fs::path defaultPath =
        fs::path(home) /
        "vanetza-pki-lab" /
        "validation-denm-pki";

    if (!fs::is_directory(defaultPath))
    {
        return {};
    }

    return defaultPath.string();
}

} // namespace ns3::denm_test
