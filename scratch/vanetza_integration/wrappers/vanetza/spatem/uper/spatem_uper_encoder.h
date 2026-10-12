#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C"
{
#include "vanetza/asn1/its/SPATEM.h"
}

struct SpatemUperEncodeResult
{
    bool success = false;
    std::vector<uint8_t> bytes;
    std::string failedType;
    std::size_t encodedBits = 0;
};

class SpatemUperEncoder
{
public:
    static SpatemUperEncodeResult Encode(
        const SPATEM_t& spatem);
};
