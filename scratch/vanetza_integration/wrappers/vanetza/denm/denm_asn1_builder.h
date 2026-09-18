#pragma once

#include <memory>

#include "vanetza_integration/denm/types/denm_input.h"

extern "C"
{
#include "vanetza/asn1/its/DENM.h"
}

class DenmAsn1Builder
{
public:
    struct AsnDeleter
    {
        void operator()(DENM_t* ptr) const;
    };

    using DenmPtr = std::unique_ptr<DENM_t, AsnDeleter>;

    static DenmPtr Build(const DenmInput& input);
};
