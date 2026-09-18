#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>


extern "C"
{
#include "vanetza/asn1/its/CAM.h"
}

struct CamUperDecodeResult
{
    bool success = false;

    std::size_t consumedBits = 0;

    std::unique_ptr<CAM_t, void(*)(CAM_t*)> cam{
        nullptr,
        [](CAM_t* ptr)
        {
            if (ptr)
            {
                ASN_STRUCT_FREE(asn_DEF_CAM, ptr);
            }
        }
    };

    std::string error;
};

class CamUperDecoder
{
public:

    static CamUperDecodeResult Decode(
        const std::vector<uint8_t>& payload);
};
