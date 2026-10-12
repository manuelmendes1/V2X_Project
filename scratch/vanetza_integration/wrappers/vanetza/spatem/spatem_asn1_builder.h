#pragma once

#include <memory>

#include "vanetza_integration/spatem/types/spatem_input.h"

extern "C"
{
#include "vanetza/asn1/its/SPATEM.h"
}

class SpatemAsn1Builder
{
public:
    struct AsnDeleter
    {
        void operator()(SPATEM_t* ptr) const;
    };

    using SpatemPtr = std::unique_ptr<SPATEM_t, AsnDeleter>;

    static SpatemPtr Build(const SpatemInput& input);
};
