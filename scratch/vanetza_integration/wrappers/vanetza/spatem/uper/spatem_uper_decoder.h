#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

extern "C"
{
#include "vanetza/asn1/its/SPATEM.h"
}

struct SpatemUperDecodeResult
{
    bool success = false;
    std::size_t consumedBits = 0;

    std::unique_ptr<SPATEM_t, void(*)(SPATEM_t*)> spatem{
        nullptr,
        [](SPATEM_t* ptr)
        {
            if (ptr)
            {
                ASN_STRUCT_FREE(
                    asn_DEF_SPATEM,
                    ptr);
            }
        }
    };

    std::string error;
};

class SpatemUperDecoder
{
public:
    static SpatemUperDecodeResult Decode(
        const std::vector<uint8_t>& payload);
};
