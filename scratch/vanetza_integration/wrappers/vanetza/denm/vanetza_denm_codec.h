#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vanetza_integration/denm/types/denm_input.h"
#include "vanetza_integration/denm/types/denm_message.h"

struct VanetzaDenmEncodeResult
{
    bool success = false;
    std::vector<uint8_t> bytes;
    std::string failedType;
};

struct VanetzaDenmDecodeResult
{
    bool success = false;
    DenmMessage message{};
    std::string error;
};

class VanetzaDenmCodec
{
public:
    static VanetzaDenmEncodeResult Encode(
        const DenmInput& input);

    static VanetzaDenmDecodeResult Decode(
        const std::vector<uint8_t>& payload);
};
