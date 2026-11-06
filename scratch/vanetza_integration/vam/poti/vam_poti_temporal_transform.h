#pragma once

#include <cstdint>
#include <string>

#include "vanetza_integration/vam/poti/vam_poti_sample.h"

struct VamPotiTimeReference
{
    std::int64_t positionTimestampItsUs = 0;
    std::int64_t generationTimestampItsUs = 0;
    std::int64_t positionAgeUs = 0;
};

class VamPotiTemporalTransform
{
public:
    static bool BuildReference(
        std::int64_t itsStartClockUs,
        const VamPotiSample& sample,
        VamPotiTimeReference& output,
        std::string& error);

    // VAM GenerationDeltaTime: position reference timestamp in
    // milliseconds since the ITS epoch, modulo 2^16.
    static bool ComputeGenerationDeltaTime(
        std::int64_t positionTimestampItsUs,
        std::uint16_t& outputGenerationDeltaTime,
        std::string& error);
};
