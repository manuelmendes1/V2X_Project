#pragma once

#include "vanetza_integration/vam/types/vam_input.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vanetza_integration
{
namespace vam
{

struct VamUperEncodeOptions
{
    // Controlled by VamGenerationManager's low-frequency scheduling policy.
    bool includeLowFrequencyContainer{false};
};

struct VamUperEncodeResult
{
    bool success{false};
    std::vector<std::uint8_t> bytes;
    std::size_t encodedBits{0};
    std::string error;
};

class VamUperEncoder
{
public:
    // Encodes the explicitly supported VAM R2 Profile 1 (pedestrian) subset.
    // Unsupported semantic extensions are rejected instead of being dropped.
    static VamUperEncodeResult Encode(
        const VamInput& input,
        VamUperEncodeOptions options = {});
};

} // namespace vam
} // namespace vanetza_integration
