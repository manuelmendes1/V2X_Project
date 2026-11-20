#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{

bool Check(bool condition, const char* name, bool& passed)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << name << '\n';

    passed = passed && condition;
    return condition;
}

bool SamePosition(
    const ns3::SumoWgs84Position& lhs,
    const ns3::SumoWgs84Position& rhs)
{
    return lhs.latitudeDeg == rhs.latitudeDeg &&
           lhs.longitudeDeg == rhs.longitudeDeg;
}

} // namespace

int main()
{
    bool passed = true;

    const ns3::SumoUtmWgs84ProjectionConfig projection{
        29,
        true,
        -526913.640,
        -4576484.190
    };

    const ns3::SumoProjectedPosition projected{
        20374.270,
        9640.360
    };

    ns3::SumoWgs84Position output{};
    std::string error = "stale error";

    const bool converted =
        ns3::SumoGeodeticTransform::ToWgs84(
            projection, projected, output, error);

    Check(
        converted,
        "FAMALICAO_UTM_REFERENCE_CONVERTS",
        passed);

    Check(
        std::abs(output.latitudeDeg - 41.425290000) <= 1e-7 &&
        std::abs(output.longitudeDeg - (-8.434076200)) <= 1e-7,
        "FAMALICAO_WGS84_REFERENCE_WITHIN_TOLERANCE",
        passed);

    Check(
        error.empty(),
        "SUCCESS_CLEARS_ERROR",
        passed);

    const ns3::SumoWgs84Position sentinel{
        12.5,
        13.5
    };

    const auto expectFailure =
        [&](const ns3::SumoUtmWgs84ProjectionConfig& config,
            const ns3::SumoProjectedPosition& position,
            const char* name)
    {
        auto preserved = sentinel;
        std::string failure;

        const bool result =
            ns3::SumoGeodeticTransform::ToWgs84(
                config, position, preserved, failure);

        return Check(
            !result &&
            !failure.empty() &&
            SamePosition(preserved, sentinel),
            name,
            passed);
    };

    auto invalidZone = projection;
    invalidZone.zone = 0;

    expectFailure(
        invalidZone,
        projected,
        "UTM_ZONE_ZERO_REJECTED_ATOMICALLY");

    invalidZone = projection;
    invalidZone.zone = 61;

    expectFailure(
        invalidZone,
        projected,
        "UTM_ZONE_ABOVE_60_REJECTED_ATOMICALLY");

    auto invalidOffset = projection;
    invalidOffset.netOffsetXMeters =
        std::numeric_limits<double>::quiet_NaN();

    expectFailure(
        invalidOffset,
        projected,
        "NON_FINITE_NETWORK_OFFSET_REJECTED");

    auto invalidPosition = projected;
    invalidPosition.xMeters =
        std::numeric_limits<double>::infinity();

    expectFailure(
        projection,
        invalidPosition,
        "NON_FINITE_PROJECTED_POSITION_REJECTED");

    invalidPosition = projected;
    invalidPosition.xMeters =
        std::numeric_limits<double>::max();

    auto overflowingOffset = projection;
    overflowingOffset.netOffsetXMeters =
        -std::numeric_limits<double>::max();

    expectFailure(
        overflowingOffset,
        invalidPosition,
        "OFFSET_SUBTRACTION_OVERFLOW_REJECTED");

    const ns3::SumoProjectedPosition outsideUtmRange{
        2000000.0,
        4586124.550
    };

    const ns3::SumoUtmWgs84ProjectionConfig zeroOffsetProjection{
        29,
        true,
        0.0,
        0.0
    };

    expectFailure(
        zeroOffsetProjection,
        outsideUtmRange,
        "GEOGRAPHICLIB_RANGE_FAILURE_PRESERVES_OUTPUT");


    const ns3::Wgs84Position supplied{
        41.425290000,
        -8.434076200
    };

    ns3::Wgs84Position resolved{};
    error = "stale";
    Check(
        ns3::SumoGeodeticTransform::ResolveWgs84(
            supplied, std::nullopt, projected, resolved, error),
        "EXPLICIT_WGS84_ACCEPTED_WITHOUT_PROJECTION",
        passed);
    Check(
        SamePosition(resolved, supplied) && error.empty(),
        "EXPLICIT_WGS84_PRESERVED",
        passed);

    resolved = {};
    error = "stale";
    Check(
        ns3::SumoGeodeticTransform::ResolveWgs84(
            std::nullopt, projection, projected, resolved, error),
        "UTM_PROJECTION_USED_WHEN_GEOGRAPHIC_SAMPLE_ABSENT",
        passed);
    Check(
        std::abs(resolved.latitudeDeg - supplied.latitudeDeg) <= 1e-7 &&
        std::abs(resolved.longitudeDeg - supplied.longitudeDeg) <= 1e-7,
        "UTM_FALLBACK_MATCHES_WGS84_GOLDEN",
        passed);

    resolved = {};
    error = "stale";
    Check(
        ns3::SumoGeodeticTransform::ResolveWgs84(
            supplied, projection, projected, resolved, error),
        "GEOGRAPHIC_AND_PROJECTED_SOURCES_CROSS_VALIDATE",
        passed);

    const auto expectResolveFailure =
        [&](const std::optional<ns3::Wgs84Position>& geographic,
            const std::optional<ns3::SumoUtmWgs84ProjectionConfig>& config,
            const char* name)
    {
        auto preserved = sentinel;
        std::string failure;
        const bool result =
            ns3::SumoGeodeticTransform::ResolveWgs84(
                geographic, config, projected, preserved, failure);
        return Check(
            !result &&
            !failure.empty() &&
            SamePosition(preserved, sentinel),
            name,
            passed);
    };

    expectResolveFailure(
        ns3::Wgs84Position{10.0, 20.0},
        projection,
        "MISMATCHED_WGS84_AND_UTM_SOURCES_REJECTED_ATOMICALLY");

    expectResolveFailure(
        std::nullopt,
        std::nullopt,
        "MISSING_WGS84_SOURCES_REJECTED_ATOMICALLY");

    expectResolveFailure(
        ns3::Wgs84Position{
            std::numeric_limits<double>::quiet_NaN(),
            -8.434076200
        },
        std::nullopt,
        "INVALID_EXPLICIT_WGS84_REJECTED_ATOMICALLY");

    std::cout << "SUMO_GEODETIC_TRANSFORM="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
