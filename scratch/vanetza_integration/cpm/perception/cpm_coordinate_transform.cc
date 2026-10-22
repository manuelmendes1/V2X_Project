#include "vanetza_integration/cpm/perception/cpm_coordinate_transform.h"

#include <cmath>

namespace
{

bool IsFinitePosition(const CpmLocalPosition& position)
{
    if (!std::isfinite(position.xMeters) ||
        !std::isfinite(position.yMeters))
    {
        return false;
    }

    return !position.zMeters.has_value() ||
           std::isfinite(*position.zMeters);
}

} // namespace

bool
CpmCoordinateTransform::ToEnuRelative(
    const CpmLocalPosition& stationPosition,
    const CpmLocalPosition& objectPosition,
    CpmCartesianPosition3D& output,
    std::string& error)
{
    if (!IsFinitePosition(stationPosition) ||
        !IsFinitePosition(objectPosition))
    {
        error = "Station and object coordinates must be finite";
        return false;
    }

    const double xEast =
        objectPosition.xMeters - stationPosition.xMeters;

    const double yNorth =
        objectPosition.yMeters - stationPosition.yMeters;

    if (!std::isfinite(xEast) || !std::isfinite(yNorth))
    {
        error = "Relative horizontal coordinates are not finite";
        return false;
    }

    std::optional<double> zUp;

    if (stationPosition.zMeters.has_value() &&
        objectPosition.zMeters.has_value())
    {
        const double deltaZ =
            *objectPosition.zMeters - *stationPosition.zMeters;

        if (!std::isfinite(deltaZ))
        {
            error = "Relative vertical coordinate is not finite";
            return false;
        }

        zUp = deltaZ;
    }

    // Só atualizar o output depois de todas as validações.
    output = CpmCartesianPosition3D{xEast, yNorth, zUp};

    error.clear();
    return true;
}
