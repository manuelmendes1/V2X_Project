#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"

#include <GeographicLib/UTMUPS.hpp>

#include <cmath>
#include <exception>

namespace ns3
{
namespace
{

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}

} // namespace

bool
SumoGeodeticTransform::ToWgs84(
    const SumoUtmWgs84ProjectionConfig& projection,
    const SumoProjectedPosition& projected,
    SumoWgs84Position& output,
    std::string& error)
{
    error.clear();

    if (projection.zone < 1 || projection.zone > 60)
    {
        return Reject(error, "UTM zone must be within [1, 60]");
    }

    if (!std::isfinite(projection.netOffsetXMeters) ||
        !std::isfinite(projection.netOffsetYMeters))
    {
        return Reject(error, "SUMO network offset must be finite");
    }

    if (!std::isfinite(projected.xMeters) ||
        !std::isfinite(projected.yMeters))
    {
        return Reject(error, "SUMO projected position must be finite");
    }

    // SUMO network coordinates include netOffset:
    // projected = UTM + netOffset.
    // Therefore the inverse operation must subtract the offset.
    const double easting =
        projected.xMeters - projection.netOffsetXMeters;

    const double northing =
        projected.yMeters - projection.netOffsetYMeters;

    if (!std::isfinite(easting) || !std::isfinite(northing))
    {
        return Reject(
            error,
            "Removing SUMO network offset produced non-finite UTM coordinates");
    }

    SumoWgs84Position candidate{};

    try
    {
        GeographicLib::UTMUPS::Reverse(
            projection.zone,
            projection.northernHemisphere,
            easting,
            northing,
            candidate.latitudeDeg,
            candidate.longitudeDeg);
    }
    catch (const std::exception&)
    {
        return Reject(
            error,
            "GeographicLib rejected the UTM coordinates");
    }

    if (!std::isfinite(candidate.latitudeDeg) ||
        !std::isfinite(candidate.longitudeDeg) ||
        candidate.latitudeDeg < -90.0 ||
        candidate.latitudeDeg > 90.0 ||
        candidate.longitudeDeg < -180.0 ||
        candidate.longitudeDeg > 180.0)
    {
        return Reject(
            error,
            "UTM reverse projection produced invalid WGS84 coordinates");
    }

    // Commit only after all validation and projection steps succeed.
    output = candidate;
    error.clear();
    return true;
}


bool
SumoGeodeticTransform::ValidateWgs84(
    const Wgs84Position& position,
    std::string& error)
{
    error.clear();
    if (!std::isfinite(position.latitudeDeg) ||
        position.latitudeDeg < -90.0 ||
        position.latitudeDeg > 90.0 ||
        !std::isfinite(position.longitudeDeg) ||
        position.longitudeDeg < -180.0 ||
        position.longitudeDeg > 180.0)
    {
        return Reject(
            error,
            "WGS84 latitude/longitude must be finite and within geographic bounds");
    }
    return true;
}

bool
SumoGeodeticTransform::ResolveWgs84(
    const std::optional<Wgs84Position>& suppliedWgs84,
    const std::optional<SumoUtmWgs84ProjectionConfig>& projection,
    const SumoProjectedPosition& projected,
    Wgs84Position& output,
    std::string& error)
{
    error.clear();

    if (suppliedWgs84.has_value() &&
        !ValidateWgs84(*suppliedWgs84, error))
    {
        return false;
    }

    std::optional<Wgs84Position> projectedWgs84;
    if (projection.has_value())
    {
        Wgs84Position candidate{};
        if (!ToWgs84(*projection, projected, candidate, error))
        {
            return false;
        }
        projectedWgs84 = candidate;
    }

    if (!suppliedWgs84.has_value() && !projectedWgs84.has_value())
    {
        return Reject(
            error,
            "WGS84 unavailable: neither a SUMO geographic sample nor a UTM/WGS84 network projection is configured");
    }

    if (suppliedWgs84.has_value() && projectedWgs84.has_value())
    {
        // SUMO serializes WGS84 to 1e-7 degrees and projected x/y to mm.
        // Allow rounding, but reject a mismatched zone, hemisphere or offset.
        constexpr double kMaximumCoordinateDifferenceDeg = 2.0e-6;
        if (std::abs(suppliedWgs84->latitudeDeg -
                     projectedWgs84->latitudeDeg) >
                kMaximumCoordinateDifferenceDeg ||
            std::abs(suppliedWgs84->longitudeDeg -
                     projectedWgs84->longitudeDeg) >
                kMaximumCoordinateDifferenceDeg)
        {
            return Reject(
                error,
                "SUMO WGS84 sample disagrees with the configured UTM/WGS84 network projection");
        }
    }

    output = suppliedWgs84.has_value()
        ? *suppliedWgs84
        : *projectedWgs84;
    error.clear();
    return true;
}

} // namespace ns3
