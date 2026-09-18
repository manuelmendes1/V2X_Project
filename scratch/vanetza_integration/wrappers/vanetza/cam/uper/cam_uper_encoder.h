#pragma once

#include <string>
#include <vector>

extern "C"
{
#include "vanetza/asn1/its/CAM.h"
}

struct CamUperResult
{
    bool success = false;

    std::vector<uint8_t> bytes;

    std::string failedType;

    std::size_t encodedBits = 0;
};

class CamUperEncoder
{
public:

    static CamUperResult Encode(
        const CAM_t& cam);
};
