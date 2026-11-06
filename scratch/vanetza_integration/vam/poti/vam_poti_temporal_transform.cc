#include "vanetza_integration/vam/poti/vam_poti_temporal_transform.h"

#include <cstdint>
#include <limits>

#include "vanetza_integration/vam/poti/vam_poti_validation.h"

bool VamPotiTemporalTransform::ComputeGenerationDeltaTime(
    std::int64_t positionTimestampItsUs,
    std::uint16_t& outputGenerationDeltaTime,
    std::string& error)
{
    error.clear();

    if (positionTimestampItsUs < 0)
    {
        error = "Position timestamp must be non-negative";
        return false;
    }

    constexpr std::int64_t microsecondsPerMillisecond = 1000;
    constexpr std::int64_t generationDeltaTimeModulus = 65536;

    const std::int64_t positionTimestampItsMs =
        positionTimestampItsUs / microsecondsPerMillisecond;

    const auto candidate = static_cast<std::uint16_t>(
        positionTimestampItsMs % generationDeltaTimeModulus);

    outputGenerationDeltaTime = candidate;
    return true;
}

bool VamPotiTemporalTransform::BuildReference(
    std::int64_t itsStartClockUs,
    const VamPotiSample& sample,
    VamPotiTimeReference& output,
    std::string& error)
{
    error.clear();

    const auto reject = [&error](const char* reason) {
        error = reason;
        return false;
    };

    if (itsStartClockUs < 0)
    {
        return reject("ITS start clock must be non-negative");
    }

    if (!VamPotiValidation::Validate(sample, error))
    {
        return false;
    }

    const std::uint64_t ageUs =
        *sample.generationTimeSimulationUs -
        *sample.positionTimeSimulationUs;

    // TS 103 300-3 V2.3.1: strictly less than 32,767 ms.
    constexpr std::uint64_t maxPositionAgeUs = 32767000;

    if (ageUs >= maxPositionAgeUs)
    {
        return reject("position age must be strictly below 32767 ms");
    }

    const auto maxClock =
        std::numeric_limits<std::int64_t>::max();

    const auto maxStartForSimulation =
        static_cast<std::uint64_t>(maxClock - itsStartClockUs);

    if (*sample.generationTimeSimulationUs > maxStartForSimulation)
    {
        return reject("ITS generation timestamp addition overflows int64");
    }

    const auto positionTimestamp =
        itsStartClockUs +
        static_cast<std::int64_t>(*sample.positionTimeSimulationUs);

    const auto generationTimestamp =
        itsStartClockUs +
        static_cast<std::int64_t>(*sample.generationTimeSimulationUs);

    VamPotiTimeReference candidate;
    candidate.positionTimestampItsUs = positionTimestamp;
    candidate.generationTimestampItsUs = generationTimestamp;
    candidate.positionAgeUs = static_cast<std::int64_t>(ageUs);

    // Commit output only after every validation has succeeded.
    output = candidate;
    return true;
}
