#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C"
{
#include "vanetza/asn1/its/DENM.h"
}

struct DenmUperEncodeResult
{
    bool success = false;
    std::vector<uint8_t> bytes;
    std::string failedType;
    std::size_t encodedBits = 0;
};

class DenmUperEncoder
{
public:
    static DenmUperEncodeResult Encode(
        const DENM_t& denm);
};
