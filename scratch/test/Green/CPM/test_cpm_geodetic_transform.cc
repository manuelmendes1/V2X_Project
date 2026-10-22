#include "vanetza_integration/cpm/perception/cpm_geodetic_transform.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << label << '\n';
    return condition;
}

bool Near(double actual, double expected, double tolerance)
{
    return std::isfinite(actual) &&
           std::abs(actual - expected) <= tolerance;
}

bool RejectsAndPreservesOutput(
    const char* label,
    const CpmGeodeticPosition& station,
    const CpmGeodeticPosition& object)
{
    CpmCartesianPosition3D output{123.0, 456.0, 789.0};
    std::string error = "previous error";

    const bool accepted =
        CpmGeodeticTransform::ToEnuRelative(
            station, object, output, error);

    const bool preserved =
        output.xEastMeters == 123.0 &&
        output.yNorthMeters == 456.0 &&
        output.zUpMeters.has_value() &&
        *output.zUpMeters == 789.0;

    return Check(
        !accepted && !error.empty() && preserved,
        label);
}

} // namespace

int main()
{
    bool allPassed = true;

    // Origem WGS84 de teste, não associada à rede SUMO.
    const CpmGeodeticPosition origin{0.0, 0.0, 0.0};
    std::string error;

    CpmCartesianPosition3D result{};
    error = "stale error";

    bool ok = CpmGeodeticTransform::ToEnuRelative(
        origin, origin, result, error);

    allPassed &= Check(
        ok &&
        Near(result.xEastMeters, 0.0, 1e-8) &&
        Near(result.yNorthMeters, 0.0, 1e-8) &&
        result.zUpMeters.has_value() &&
        Near(*result.zUpMeters, 0.0, 1e-8) &&
        error.empty(),
        "origin_maps_to_zero_and_clears_error");

    CpmCartesianPosition3D east{};
    ok = CpmGeodeticTransform::ToEnuRelative(
        origin, CpmGeodeticPosition{0.0, 0.001, 0.0},
        east, error);

    allPassed &= Check(
        ok &&
        east.xEastMeters > 111.0 &&
        east.xEastMeters < 112.0 &&
        Near(east.yNorthMeters, 0.0, 0.01),
        "east_has_positive_east_component");

    CpmCartesianPosition3D west{};
    ok = CpmGeodeticTransform::ToEnuRelative(
        origin, CpmGeodeticPosition{0.0, -0.001, 0.0},
        west, error);

    allPassed &= Check(
        ok &&
        west.xEastMeters < -111.0 &&
        west.xEastMeters > -112.0 &&
        Near(west.yNorthMeters, 0.0, 0.01),
        "west_has_negative_east_component");

    CpmCartesianPosition3D north{};
    ok = CpmGeodeticTransform::ToEnuRelative(
        origin, CpmGeodeticPosition{0.001, 0.0, 0.0},
        north, error);

    allPassed &= Check(
        ok &&
        north.yNorthMeters > 110.0 &&
        north.yNorthMeters < 111.0 &&
        Near(north.xEastMeters, 0.0, 0.01),
        "north_has_positive_north_component");

    CpmCartesianPosition3D south{};
    ok = CpmGeodeticTransform::ToEnuRelative(
        origin, CpmGeodeticPosition{-0.001, 0.0, 0.0},
        south, error);

    allPassed &= Check(
        ok &&
        south.yNorthMeters < -110.0 &&
        south.yNorthMeters > -111.0 &&
        Near(south.xEastMeters, 0.0, 0.01),
        "south_has_negative_north_component");

    CpmCartesianPosition3D vertical{};
    ok = CpmGeodeticTransform::ToEnuRelative(
        CpmGeodeticPosition{45.0, 9.0, 100.0},
        CpmGeodeticPosition{45.0, 9.0, 112.0},
        vertical, error);

    allPassed &= Check(
        ok &&
        Near(vertical.xEastMeters, 0.0, 1e-6) &&
        Near(vertical.yNorthMeters, 0.0, 1e-6) &&
        vertical.zUpMeters.has_value() &&
        Near(*vertical.zUpMeters, 12.0, 1e-6),
        "ellipsoidal_height_difference_maps_to_up");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    allPassed &= RejectsAndPreservesOutput(
        "rejects_nan_station_latitude_atomically",
        CpmGeodeticPosition{nan, 0.0, 0.0}, origin);

    allPassed &= RejectsAndPreservesOutput(
        "rejects_station_latitude_out_of_range_atomically",
        CpmGeodeticPosition{90.001, 0.0, 0.0}, origin);

    allPassed &= RejectsAndPreservesOutput(
        "rejects_object_longitude_out_of_range_atomically",
        origin, CpmGeodeticPosition{0.0, 180.001, 0.0});

    allPassed &= RejectsAndPreservesOutput(
        "rejects_infinite_object_height_atomically",
        origin, CpmGeodeticPosition{0.0, 0.0, inf});

    allPassed &= RejectsAndPreservesOutput(
        "rejects_non_finite_station_longitude_atomically",
        CpmGeodeticPosition{0.0, nan, 0.0}, origin);

    // Referências independentes calculadas com PROJ cct.
    // Origem: RSU CPM configurada no cenário Famalicão.
    // Estes valores verificam especificamente o caminho horizontal usado
    // por CpmVehiclePerceptionSource em produção.
    const CpmGeodeticHorizontalPosition horizontalStation{
        41.42528995, -8.43407620};
    const CpmGeodeticHorizontalPosition horizontalEast{
        41.42528995, -8.43397620};
    const CpmGeodeticHorizontalPosition horizontalNorth{
        41.42538995, -8.43407620};

    CpmCartesianPosition3D horizontalEastResult{};
    ok = CpmGeodeticTransform::ToEnuHorizontalRelative(
        horizontalStation, horizontalEast, horizontalEastResult, error);

    allPassed &= Check(
        ok &&
        Near(horizontalEastResult.xEastMeters, 8.35920578307, 1e-4) &&
        Near(horizontalEastResult.yNorthMeters, 0.00000482654, 1e-4) &&
        !horizontalEastResult.zUpMeters.has_value() &&
        error.empty(),
        "horizontal_proj_reference_east");

    CpmCartesianPosition3D horizontalNorthResult{};
    ok = CpmGeodeticTransform::ToEnuHorizontalRelative(
        horizontalStation, horizontalNorth, horizontalNorthResult, error);

    allPassed &= Check(
        ok &&
        Near(horizontalNorthResult.xEastMeters, 0.0, 1e-4) &&
        Near(horizontalNorthResult.yNorthMeters, 11.1062138237, 1e-4) &&
        !horizontalNorthResult.zUpMeters.has_value() &&
        error.empty(),
        "horizontal_proj_reference_north");

    std::cout << (allPassed
        ? "CPM_GEODETIC_TRANSFORM_TEST=PASS\n"
        : "CPM_GEODETIC_TRANSFORM_TEST=FAIL\n");

    return allPassed ? 0 : 1;
}
