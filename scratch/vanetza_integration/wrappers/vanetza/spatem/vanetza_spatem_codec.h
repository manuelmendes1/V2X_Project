#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vanetza_integration/spatem/types/spatem_input.h"
#include "vanetza_integration/spatem/types/spatem_message.h"

struct VanetzaSpatemEncodeResult
{
    bool success = false;
    std::vector<uint8_t> bytes;
    std::string error;
    std::size_t encodedBits = 0;
};

struct VanetzaSpatemDecodeResult
{
    bool success = false;
    SpatemMessage message{};
    std::string error;
    std::size_t consumedBits = 0;
};

class VanetzaSpatemCodec
{
public:
    static VanetzaSpatemEncodeResult Encode(
        const SpatemInput& input);

    static VanetzaSpatemDecodeResult Decode(
        const std::vector<uint8_t>& payload);
};
