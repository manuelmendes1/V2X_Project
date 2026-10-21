#ifndef VANETZA_INTEGRATION_CPM_VALIDATION_CPM_MANAGEMENT_METADATA_VALIDATION_H
#define VANETZA_INTEGRATION_CPM_VALIDATION_CPM_MANAGEMENT_METADATA_VALIDATION_H

#include <cstdint>

#include "vanetza_integration/cpm/types/cpm_domain_types.h"

enum class CpmManagementMetadataValidationError : std::uint8_t
{
    None,
    SegmentationTotalOutOfRange,
    SegmentationOrdinalOutOfRange,
    SegmentationOrdinalExceedsTotal,
    MessageRateMinMantissaOutOfRange,
    MessageRateMinExponentOutOfRange,
    MessageRateMaxMantissaOutOfRange,
    MessageRateMaxExponentOutOfRange,
    MessageRateMinExceedsMax
};

namespace cpm_management_metadata_detail
{

constexpr std::int64_t PowerOfTen(unsigned exponent) noexcept
{
    std::int64_t value = 1;
    for (unsigned i = 0; i < exponent; ++i) {
        value *= 10;
    }
    return value;
}

// Chamado apenas depois de validar o expoente [-5, 2].
// Representa a taxa em unidades comuns de 10^-5 Hz.
inline std::int64_t RateAtCommonScale(
    const CpmMessageRateHz& rate) noexcept
{
    const auto scaleExponent =
        static_cast<unsigned>(static_cast<int>(rate.exponent) + 5);

    return static_cast<std::int64_t>(rate.mantissa) *
           PowerOfTen(scaleExponent);
}

} // namespace cpm_management_metadata_detail

inline CpmManagementMetadataValidationError
ValidateCpmManagementMetadata(const CpmSemanticData& data) noexcept
{
    using Error = CpmManagementMetadataValidationError;

    if (data.segmentationInfo) {
        const auto& segmentation = *data.segmentationInfo;

        if (segmentation.totalMsgNo < 1 ||
            segmentation.totalMsgNo > 8) {
            return Error::SegmentationTotalOutOfRange;
        }

        if (segmentation.thisMsgNo < 1 ||
            segmentation.thisMsgNo > 8) {
            return Error::SegmentationOrdinalOutOfRange;
        }

        if (segmentation.thisMsgNo > segmentation.totalMsgNo) {
            return Error::SegmentationOrdinalExceedsTotal;
        }
    }

    if (data.messageRateRange) {
        const auto& minimum = data.messageRateRange->messageRateMin;
        const auto& maximum = data.messageRateRange->messageRateMax;

        if (minimum.mantissa < 1 || minimum.mantissa > 100) {
            return Error::MessageRateMinMantissaOutOfRange;
        }
        if (minimum.exponent < -5 || minimum.exponent > 2) {
            return Error::MessageRateMinExponentOutOfRange;
        }
        if (maximum.mantissa < 1 || maximum.mantissa > 100) {
            return Error::MessageRateMaxMantissaOutOfRange;
        }
        if (maximum.exponent < -5 || maximum.exponent > 2) {
            return Error::MessageRateMaxExponentOutOfRange;
        }

        if (cpm_management_metadata_detail::RateAtCommonScale(minimum) >
            cpm_management_metadata_detail::RateAtCommonScale(maximum)) {
            return Error::MessageRateMinExceedsMax;
        }
    }

    return Error::None;
}

#endif
