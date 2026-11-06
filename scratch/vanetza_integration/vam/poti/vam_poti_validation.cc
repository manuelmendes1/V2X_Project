#include "vanetza_integration/vam/poti/vam_poti_validation.h"

#include <cmath>
#include <cstdint>
#include <limits>

bool VamPotiValidation::Validate(
    const VamPotiSample& sample,
    std::string& error)
{
    error.clear();

    const auto reject = [&error](const char* reason) {
        error = reason;
        return false;
    };

    if (!sample.latitudeDeg.has_value())
    {
        return reject("latitude is required");
    }

    if (!sample.longitudeDeg.has_value())
    {
        return reject("longitude is required");
    }

    if (!sample.speedMps.has_value())
    {
        return reject("speed is required");
    }

    if (!sample.headingDeg.has_value())
    {
        return reject("heading is required");
    }

    if (!sample.positionTimeSimulationUs.has_value())
    {
        return reject("position timestamp is required");
    }

    if (!sample.generationTimeSimulationUs.has_value())
    {
        return reject("generation timestamp is required");
    }

    if (!std::isfinite(*sample.latitudeDeg) ||
        *sample.latitudeDeg < -90.0 ||
        *sample.latitudeDeg > 90.0)
    {
        return reject("latitude must be finite and within [-90, 90]");
    }

    if (!std::isfinite(*sample.longitudeDeg) ||
        *sample.longitudeDeg < -180.0 ||
        *sample.longitudeDeg > 180.0)
    {
        return reject("longitude must be finite and within [-180, 180]");
    }

    if (sample.altitudeMeters.has_value() &&
        !std::isfinite(*sample.altitudeMeters))
    {
        return reject("altitude must be finite when present");
    }

    if (!std::isfinite(*sample.speedMps) || *sample.speedMps < 0.0)
    {
        return reject("speed must be finite and non-negative");
    }

    if (!std::isfinite(*sample.headingDeg) ||
        *sample.headingDeg < 0.0 ||
        *sample.headingDeg > 360.0)
    {
        return reject("SUMO heading must be finite and within [0, 360]");
    }

    if (sample.longitudinalAccelerationMps2.has_value() &&
        !std::isfinite(*sample.longitudinalAccelerationMps2))
    {
        return reject("longitudinal acceleration must be finite when present");
    }

    constexpr auto maxTimestamp =
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max());

    if (*sample.positionTimeSimulationUs > maxTimestamp ||
        *sample.generationTimeSimulationUs > maxTimestamp)
    {
        return reject("simulation timestamp exceeds int64 range");
    }

    if (*sample.generationTimeSimulationUs <
        *sample.positionTimeSimulationUs)
    {
        return reject("generation time precedes position acquisition");
    }

    return true;
}
