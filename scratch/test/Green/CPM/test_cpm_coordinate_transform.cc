#include "vanetza_integration/cpm/perception/cpm_coordinate_transform.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace
{

bool Near(double actual, double expected)
{
    return std::isfinite(actual) &&
           std::abs(actual - expected) <= 1e-9;
}

bool CheckTransform(
    const char* name,
    const CpmLocalPosition& station,
    const CpmLocalPosition& object,
    double expectedX,
    double expectedY,
    std::optional<double> expectedZ)
{
    CpmCartesianPosition3D output{};
    std::string error;

    if (!CpmCoordinateTransform::ToEnuRelative(
            station, object, output, error))
    {
        std::cerr << "FAIL " << name
                  << ": transformation rejected: " << error << '\n';
        return false;
    }

    if (!error.empty() ||
        !Near(output.xEastMeters, expectedX) ||
        !Near(output.yNorthMeters, expectedY) ||
        output.zUpMeters.has_value() != expectedZ.has_value() ||
        (expectedZ &&
         !Near(*output.zUpMeters, *expectedZ)))
    {
        std::cerr << "FAIL " << name
                  << ": unexpected ENU coordinates"
                  << " x=" << output.xEastMeters
                  << " y=" << output.yNorthMeters;

        if (output.zUpMeters)
        {
            std::cerr << " z=" << *output.zUpMeters;
        }
        else
        {
            std::cerr << " z=UNAVAILABLE";
        }

        std::cerr << '\n';
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

bool CheckRejectsInvalidInput()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    const CpmLocalPosition station{0.0, 0.0, 0.0};
    const CpmLocalPosition invalidObject{nan, 2.0, 1.0};

    CpmCartesianPosition3D output{777.0, 888.0, 999.0};
    std::string error;

    const bool accepted = CpmCoordinateTransform::ToEnuRelative(
        station, invalidObject, output, error);

    if (accepted ||
        error.empty() ||
        !Near(output.xEastMeters, 777.0) ||
        !Near(output.yNorthMeters, 888.0) ||
        !output.zUpMeters ||
        !Near(*output.zUpMeters, 999.0))
    {
        std::cerr
            << "FAIL INVALID_COORDINATE: expected rejection, error and "
               "unchanged output\n";
        return false;
    }

    std::cout << "PASS INVALID_COORDINATE\n";
    return true;
}

bool CheckRejectsNonFiniteAltitude()
{
    const double infinity =
        std::numeric_limits<double>::infinity();

    const CpmLocalPosition station{0.0, 0.0, 0.0};
    const CpmLocalPosition invalidObject{1.0, 2.0, infinity};

    CpmCartesianPosition3D output{};
    std::string error;

    if (CpmCoordinateTransform::ToEnuRelative(
            station, invalidObject, output, error) ||
        error.empty())
    {
        std::cerr << "FAIL NON_FINITE_ALTITUDE\n";
        return false;
    }

    std::cout << "PASS NON_FINITE_ALTITUDE\n";
    return true;
}

} // namespace

int main()
{
    bool passed = true;

    // Origem e eixos cardeais: x=Este, y=Norte.
    const CpmLocalPosition origin{0.0, 0.0, 0.0};

    passed = CheckTransform(
        "NORTH", origin, {0.0, 10.0, 0.0},
        0.0, 10.0, 0.0) && passed;

    passed = CheckTransform(
        "SOUTH", origin, {0.0, -10.0, 0.0},
        0.0, -10.0, 0.0) && passed;

    passed = CheckTransform(
        "EAST", origin, {10.0, 0.0, 0.0},
        10.0, 0.0, 0.0) && passed;

    passed = CheckTransform(
        "WEST", origin, {-10.0, 0.0, 0.0},
        -10.0, 0.0, 0.0) && passed;

    // A transformação deve subtrair a referência e preservar o delta Z.
    passed = CheckTransform(
        "KNOWN_3D_OFFSET",
        {100.0, 200.0, 5.0},
        {130.0, 160.0, 8.0},
        30.0, -40.0, 3.0) && passed;

    // Sem altitude da estação, não é possível calcular a altitude relativa.
    passed = CheckTransform(
        "UNAVAILABLE_RELATIVE_ALTITUDE",
        {0.0, 0.0, std::nullopt},
        {0.0, 12.0, 3.0},
        0.0, 12.0, std::nullopt) && passed;

    passed = CheckRejectsInvalidInput() && passed;
    passed = CheckRejectsNonFiniteAltitude() && passed;

    if (!passed)
    {
        return 1;
    }

    std::cout << "CPM_COORDINATE_TRANSFORM=PASS\n";
    return 0;
}
