#pragma once

#include <cstdint>

#include "vanetza_integration/spatem/types/spatem_message.h"

extern "C"
{
#include "vanetza/asn1/its/SPATEM.h"
}

class SpatemConverter
{
public:
    static SpatemMessage ToMessage(
        const SPATEM_t& spatem);
};
