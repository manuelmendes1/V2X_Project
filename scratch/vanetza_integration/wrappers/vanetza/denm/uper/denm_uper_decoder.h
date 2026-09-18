#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

extern "C"
{
#include "vanetza/asn1/its/DENM.h"
}

struct DenmUperDecodeResult
{
    bool success = false;
    std::size_t consumedBits = 0;

    std::unique_ptr<DENM_t, void(*)(DENM_t*)> denm{
        nullptr,
        [](DENM_t* ptr)
        {
            if (ptr)
            {
                ASN_STRUCT_FREE(asn_DEF_DENM, ptr);
            }
        }
    };

    std::string error;
};

class DenmUperDecoder
{
public:
    static DenmUperDecodeResult Decode(
        const std::vector<uint8_t>& payload);
};
