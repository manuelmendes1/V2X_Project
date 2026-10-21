#ifndef VANETZA_INTEGRATION_CPM_VALIDATION_CPM_CONFIDENCE_RATIO_VALIDATION_H
#define VANETZA_INTEGRATION_CPM_VALIDATION_CPM_CONFIDENCE_RATIO_VALIDATION_H

#include <cmath>
#include <cstdint>

#include "vanetza_integration/cpm/types/cpm_domain_types.h"

enum class CpmConfidenceRatioValidationError : std::uint8_t
{
    None,
    ObjectClassRatioNonFinite,
    ObjectClassRatioOutOfRange,
    SensorRatioNonFinite,
    SensorRatioOutOfRange,
    PerceptionRegionRatioNonFinite,
    PerceptionRegionRatioOutOfRange
};

// Valida a representação neutra [0.0, 1.0].
// Não valida a codificabilidade do valor no ASN.1 de destino.
inline CpmConfidenceRatioValidationError
ValidateCpmConfidenceRatios(const CpmSemanticData& data) noexcept
{
    using Error = CpmConfidenceRatioValidationError;

    for (const auto& object : data.perceivedObjects) {
        if (!object.classifications) {
            continue;
        }

        for (const auto& classification : *object.classifications) {
            if (!classification.confidenceRatio) {
                continue;
            }

            const double value = classification.confidenceRatio->value;
            if (!std::isfinite(value)) {
                return Error::ObjectClassRatioNonFinite;
            }
            if (value < 0.0 || value > 1.0) {
                return Error::ObjectClassRatioOutOfRange;
            }
        }
    }

    for (const auto& sensor : data.sensorInformation) {
        if (!sensor.perceptionRegionConfidenceRatio) {
            continue;
        }

        const double value =
            sensor.perceptionRegionConfidenceRatio->value;
        if (!std::isfinite(value)) {
            return Error::SensorRatioNonFinite;
        }
        if (value < 0.0 || value > 1.0) {
            return Error::SensorRatioOutOfRange;
        }
    }

    for (const auto& region : data.perceptionRegions) {
        if (!region.confidenceRatio) {
            continue;
        }

        const double value = region.confidenceRatio->value;
        if (!std::isfinite(value)) {
            return Error::PerceptionRegionRatioNonFinite;
        }
        if (value < 0.0 || value > 1.0) {
            return Error::PerceptionRegionRatioOutOfRange;
        }
    }

    return Error::None;
}

#endif
