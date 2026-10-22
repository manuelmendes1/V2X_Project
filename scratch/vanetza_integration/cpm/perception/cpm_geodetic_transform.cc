#include "vanetza_integration/cpm/perception/cpm_geodetic_transform.h"

#include <GeographicLib/LocalCartesian.hpp>
#include <GeographicLib/Geodesic.hpp>

#include <cmath>
#include <exception>

namespace
{

bool IsValidGeodeticPosition(const CpmGeodeticPosition& position)
{
    return std::isfinite(position.latitudeDeg) &&
           std::isfinite(position.longitudeDeg) &&
           std::isfinite(position.ellipsoidHeightMeters) &&
           position.latitudeDeg >= -90.0 &&
           position.latitudeDeg <= 90.0 &&
           position.longitudeDeg >= -180.0 &&
           position.longitudeDeg <= 180.0;
}

bool IsValidGeodeticHorizontalPosition(
    const CpmGeodeticHorizontalPosition& position)
{
    return std::isfinite(position.latitudeDeg) &&
           std::isfinite(position.longitudeDeg) &&
           position.latitudeDeg >= -90.0 &&
           position.latitudeDeg <= 90.0 &&
           position.longitudeDeg >= -180.0 &&
           position.longitudeDeg <= 180.0;
}

bool Fail(std::string& error, const char* message)
{
    error = message;
    return false;
}

} // namespace

bool
CpmGeodeticTransform::ToEnuHorizontalRelative(
    const CpmGeodeticHorizontalPosition& station,
    const CpmGeodeticHorizontalPosition& object,
    CpmCartesianPosition3D& output,
    std::string& error)
{
    if (!IsValidGeodeticHorizontalPosition(station))
    {
        return Fail(error, "Station geodetic coordinates are invalid");
    }

    if (!IsValidGeodeticHorizontalPosition(object))
    {
        return Fail(error, "Object geodetic coordinates are invalid");
    }

    try
    {
        double distanceMeters = 0.0;
        double initialAzimuthDeg = 0.0;
        double finalAzimuthDeg = 0.0;

        GeographicLib::Geodesic::WGS84().Inverse(
            station.latitudeDeg,
            station.longitudeDeg,
            object.latitudeDeg,
            object.longitudeDeg,
            distanceMeters,
            initialAzimuthDeg,
            finalAzimuthDeg);

        if (!std::isfinite(distanceMeters) ||
            !std::isfinite(initialAzimuthDeg))
        {
            return Fail(
                error,
                "Horizontal geodesic result is not finite");
        }

        constexpr double kDegreesToRadians =
            3.14159265358979323846 / 180.0;

        const double azimuthRadians =
            initialAzimuthDeg * kDegreesToRadians;

        const double eastMeters =
            distanceMeters * std::sin(azimuthRadians);
        const double northMeters =
            distanceMeters * std::cos(azimuthRadians);

        if (!std::isfinite(eastMeters) ||
            !std::isfinite(northMeters))
        {
            return Fail(
                error,
                "Horizontal ENU result is not finite");
        }

        // A saída é deliberadamente horizontal: não se infere altitude.
        output = CpmCartesianPosition3D{
            eastMeters,
            northMeters,
            std::nullopt};

        error.clear();
        return true;
    }
    catch (const std::exception&)
    {
        return Fail(
            error,
            "Geodetic horizontal transformation failed");
    }
}

bool
CpmGeodeticTransform::ToEnuRelative(
    const CpmGeodeticPosition& station,
    const CpmGeodeticPosition& object,
    CpmCartesianPosition3D& output,
    std::string& error)
{
    if (!IsValidGeodeticPosition(station))
    {
        return Fail(error, "Station geodetic coordinates are invalid");
    }

    if (!IsValidGeodeticPosition(object))
    {
        return Fail(error, "Object geodetic coordinates are invalid");
    }

    try
    {
        // A estação define a origem e a orientação do referencial local.
        // GeographicLib utiliza WGS84 por omissão.
        const GeographicLib::LocalCartesian transform(
            station.latitudeDeg,
            station.longitudeDeg,
            station.ellipsoidHeightMeters);

        double eastMeters = 0.0;
        double northMeters = 0.0;
        double upMeters = 0.0;

        transform.Forward(
            object.latitudeDeg,
            object.longitudeDeg,
            object.ellipsoidHeightMeters,
            eastMeters,
            northMeters,
            upMeters);

        if (!std::isfinite(eastMeters) ||
            !std::isfinite(northMeters) ||
            !std::isfinite(upMeters))
        {
            return Fail(error, "ENU result contains non-finite coordinates");
        }

        // Só atualizar o resultado depois de todas as validações.
        output = CpmCartesianPosition3D{
            eastMeters,
            northMeters,
            upMeters};

        error.clear();
        return true;
    }
    catch (const std::exception&)
    {
        return Fail(error, "Geodetic to ENU transformation failed");
    }
}
