#pragma once

#include <memory>

#include "vanetza_integration/cam/types/cam_input.h"

extern "C"
{
#include "vanetza/asn1/its/CAM.h"
}

class CamAsn1Builder
{
public:

    struct AsnDeleter
    {
        void operator()(CAM_t* ptr) const;
    };

    using CamPtr = std::unique_ptr<CAM_t, AsnDeleter>;

    static CamPtr Build(const CamInput& input);
};
