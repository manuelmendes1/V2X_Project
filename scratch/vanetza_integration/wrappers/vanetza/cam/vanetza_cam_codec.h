#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vanetza_integration/cam/types/cam_input.h"
#include "vanetza_integration/cam/types/cam_message.h"

struct VanetzaCamEncodeResult
{
    bool success = false;
    std::vector<uint8_t> bytes;
    std::string failedType;
};

struct VanetzaCamDecodeResult
{
    bool success = false;
    CamMessage message{};
    std::string error;
};

class VanetzaCamCodec
{
public:
    static VanetzaCamEncodeResult Encode(const CamInput& input);

    static VanetzaCamDecodeResult Decode(
        const std::vector<uint8_t>& payload,
        uint64_t timestamp);
};
