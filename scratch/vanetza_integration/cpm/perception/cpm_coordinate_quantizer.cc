#include "vanetza_integration/cpm/perception/cpm_coordinate_quantizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool
CpmCoordinateQuantizer::ToAsn1Value(
    double meters,
    std::int32_t& output,
    std::string& error)
{
    if (!std::isfinite(meters))
    {
        error = "Coordinate must be finite";
        return false;
    }

    // Aplicar primeiro os limites semânticos do CDD. O extremo inferior
    // normal é exclusivo; o extremo superior normal é inclusivo.
    if (meters <= kMinNormalMetersExclusive)
    {
        output = kNegativeOutOfRange;
        error.clear();
        return true;
    }

    if (meters > kMaxNormalMetersInclusive)
    {
        output = kPositiveOutOfRange;
        error.clear();
        return true;
    }

    const long double scaled =
        static_cast<long double>(meters) * 100.0L;

    // Estabilizar fronteiras exatas de centímetros afetadas pela
    // representação binária double, por exemplo -0,29 m.
    const long double nearest = std::round(scaled);
    const long double magnitude =
        std::max(1.0L, std::abs(scaled));

    const long double tolerance =
        4.0L *
        static_cast<long double>(
            std::numeric_limits<double>::epsilon()) *
        magnitude;

    const long double quantized =
        std::abs(scaled - nearest) <= tolerance
            ? nearest
            : std::ceil(scaled);

    // A entrada já foi limitada ao intervalo normal. O clamp protege os
    // códigos reservados de eventuais resíduos de arredondamento.
    const long double bounded = std::clamp(
        quantized,
        static_cast<long double>(kMinNormalValue),
        static_cast<long double>(kMaxNormalValue));

    output = static_cast<std::int32_t>(bounded);
    error.clear();
    return true;
}
