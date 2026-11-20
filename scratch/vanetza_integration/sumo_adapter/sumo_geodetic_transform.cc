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

} // namespace ns3
