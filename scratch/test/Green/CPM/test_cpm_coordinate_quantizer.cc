#include "vanetza_integration/cpm/perception/cpm_coordinate_quantizer.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace
{

bool CheckValue(
    const char* name,
    double meters,
    std::int32_t expected)
{
    std::int32_t output = 9876;
    std::string error = "stale error";

    if (!CpmCoordinateQuantizer::ToAsn1Value(
            meters, output, error) ||
        output != expected ||
        !error.empty())
    {
        std::cerr << "FAIL " << name
                  << ": meters=" << meters
                  << " expected=" << expected
                  << " actual=" << output
                  << " error=" << error << '\n';
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

bool CheckRejectsNonFinite(
    const char* name,
    double meters)
{
    std::int32_t output = 4321;
    std::string error;

    if (CpmCoordinateQuantizer::ToAsn1Value(
            meters, output, error) ||
        output != 4321 ||
        error.empty())
    {
        std::cerr << "FAIL " << name
                  << ": expected rejection and unchanged output\n";
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

} // namespace

int main()
{
    bool passed = true;

    using Quantizer = CpmCoordinateQuantizer;

    // Quantização em intervalos de 1 cm.
    passed = CheckValue("ZERO", 0.0, 0) && passed;
    passed = CheckValue("POSITIVE_SUB_CM", 0.001, 1) && passed;
    passed = CheckValue("EXACT_POSITIVE_CM", 0.01, 1) && passed;
    passed = CheckValue("POSITIVE_ABOVE_CM", 0.0100001, 2) && passed;
    passed = CheckValue("NEGATIVE_SUB_CM", -0.001, 0) && passed;
    passed = CheckValue("EXACT_NEGATIVE_CM", -0.01, -1) && passed;
    passed = CheckValue("POSITIVE_DECIMAL_BOUNDARY", 0.29, 29) && passed;
    passed = CheckValue("NEGATIVE_DECIMAL_BOUNDARY", -0.29, -29) && passed;

    // Fronteiras normais e valores reservados de fora do intervalo.
    passed = CheckValue(
        "LOWEST_NORMAL_NEAR_BOUNDARY",
        -1310.719,
        Quantizer::kMinNormalValue) && passed;

    passed = CheckValue(
        "LOWER_BOUNDARY_EXACT_RESERVED",
        -1310.72,
        Quantizer::kNegativeOutOfRange) && passed;

    passed = CheckValue(
        "LOWER_BOUNDARY_OUT_OF_RANGE",
        -1310.7201,
        Quantizer::kNegativeOutOfRange) && passed;

    passed = CheckValue(
        "UPPER_BOUNDARY_EXACT_VALID",
        1310.70,
        Quantizer::kMaxNormalValue) && passed;

    passed = CheckValue(
        "UPPER_BOUNDARY_OUT_OF_RANGE",
        1310.7001,
        Quantizer::kPositiveOutOfRange) && passed;

    passed = CheckValue(
        "NEXT_DOUBLE_ABOVE_LOWER_BOUNDARY",
        std::nextafter(
            Quantizer::kMinNormalMetersExclusive,
            std::numeric_limits<double>::infinity()),
        Quantizer::kMinNormalValue) && passed;

    passed = CheckValue(
        "NEXT_DOUBLE_ABOVE_UPPER_BOUNDARY",
        std::nextafter(
            Quantizer::kMaxNormalMetersInclusive,
            std::numeric_limits<double>::infinity()),
        Quantizer::kPositiveOutOfRange) && passed;

    // Valores inválidos não devem substituir a saída anterior.
    passed = CheckRejectsNonFinite(
        "REJECT_NAN",
        std::numeric_limits<double>::quiet_NaN()) && passed;

    passed = CheckRejectsNonFinite(
        "REJECT_POSITIVE_INFINITY",
        std::numeric_limits<double>::infinity()) && passed;

    passed = CheckRejectsNonFinite(
        "REJECT_NEGATIVE_INFINITY",
        -std::numeric_limits<double>::infinity()) && passed;

    if (!passed)
    {
        return 1;
    }

    std::cout << "CPM_COORDINATE_QUANTIZER=PASS\n";
    return 0;
}
