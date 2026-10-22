#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <variant>

#include "vanetza_integration/cpm/perception/cpm_vehicle_perception_source.h"

namespace
{

bool Check(bool condition, const char* name)
{
    if (!condition)
    {
        std::cerr << "FAIL " << name << '\n';
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

CpmVehiclePerceptionConfig ValidConfig()
{
    return CpmVehiclePerceptionConfig{
        9001,
        41.3851057,
        -8.4330521,
        50.0,
        1'000'000'000'000'000LL};
}

CpmVehiclePerceptionObservation ValidVehicle(
    const std::string& id,
    double longitude,
    std::uint64_t timeUs)
{
    return CpmVehiclePerceptionObservation{
        id,
        CpmGeodeticHorizontalPosition{41.3851057, longitude},
        timeUs};
}

} // namespace

int main()
{
    CpmVehiclePerceptionSource source;
    const auto config = ValidConfig();

    std::vector<CpmVehiclePerceptionObservation> observations{
        ValidVehicle("vehicle-0", -8.4329, 900'000),
        {"vehicle-1", std::nullopt, 900'000},
        {"vehicle-2",
         CpmGeodeticHorizontalPosition{41.3852, -8.4328},
         std::nullopt}};

    CpmInput output{};
    std::string error;

    if (!Check(
            source.Build(config, 1'000'000, observations, output, error),
            "VALID_OBSERVATIONS_BUILD"))
    {
        std::cerr << "Build error: " << error << '\n';
        return 1;
    }

    if (!Check(output.station.stationId == 9001,
               "RSU_STATION_ID_PRESERVED"))
    {
        return 1;
    }

    if (!Check(
            std::holds_alternative<CpmRoadSideUnitStationData>(
                output.station.origin),
            "RSU_ORIGIN_SELECTED"))
    {
        return 1;
    }

    if (!Check(
            output.referencePosition.latitudeDeg.has_value() &&
            std::abs(*output.referencePosition.latitudeDeg - 41.3851057)
                < 1e-8 &&
            output.referencePosition.longitudeDeg.has_value() &&
            std::abs(*output.referencePosition.longitudeDeg - (-8.4330521))
                < 1e-8,
            "RSU_REFERENCE_POSITION_PRESERVED"))
    {
        return 1;
    }

    if (!Check(
            !output.referencePosition.altitudeMeters.has_value(),
            "UNKNOWN_RSU_ALTITUDE_REMAINS_UNAVAILABLE"))
    {
        return 1;
    }

    if (!Check(output.knownObjectCount == 3,
               "KNOWN_OBJECT_COUNT_INCLUDES_UNAVAILABLE_SAMPLES"))
    {
        return 1;
    }

    if (!Check(output.perceivedObjects.size() == 1,
               "MISSING_GEO_OR_TIME_EXCLUDED"))
    {
        return 1;
    }

    const auto firstObject = output.perceivedObjects.front();
    const auto firstObjectId = firstObject.objectId;

    if (!Check(firstObjectId != 0, "LOCAL_OBJECT_ID_NONZERO"))
    {
        return 1;
    }

    if (!Check(
            firstObject.measurementDeltaTimeMs == -100,
            "SUMO_MEASUREMENT_TIME_CONVERTED_TO_ITS"))
    {
        return 1;
    }

    if (!Check(
            firstObject.objectAgeMs.has_value() &&
            *firstObject.objectAgeMs == 0,
            "FIRST_OBSERVATION_AGE_ZERO"))
    {
        return 1;
    }

    if (!Check(
            firstObject.position.xEast.value > 0.0 &&
            std::abs(firstObject.position.yNorth.value) < 0.5,
            "GEODETIC_POSITION_CONVERTED_TO_HORIZONTAL_ENU"))
    {
        return 1;
    }

    if (!Check(
            !firstObject.position.zUp.has_value(),
            "UNKNOWN_OBJECT_ALTITUDE_REMAINS_UNAVAILABLE"))
    {
        return 1;
    }

    observations[0] =
        ValidVehicle("vehicle-0", -8.4329, 1'100'000);

    CpmInput nextOutput{};
    error.clear();

    if (!Check(
            source.Build(
                config, 1'100'000, observations, nextOutput, error),
            "SECOND_CYCLE_BUILD"))
    {
        std::cerr << "Build error: " << error << '\n';
        return 1;
    }

    if (!Check(
            nextOutput.perceivedObjects.size() == 1 &&
            nextOutput.perceivedObjects.front().objectId == firstObjectId,
            "LOCAL_OBJECT_ID_STABLE_ACROSS_CYCLES"))
    {
        return 1;
    }

    if (!Check(
            nextOutput.perceivedObjects.front().objectAgeMs.has_value() &&
            *nextOutput.perceivedObjects.front().objectAgeMs == 100,
            "OBJECT_AGE_ADVANCES_INDEPENDENTLY"))
    {
        return 1;
    }

    CpmVehiclePerceptionSource duplicateSource;
    CpmInput sentinel{};
    sentinel.station.stationId = 77;
    sentinel.referenceTimeItsMs = 123;
    error.clear();

    const std::vector<CpmVehiclePerceptionObservation> duplicates{
        ValidVehicle("duplicate", -8.4329, 900'000),
        ValidVehicle("duplicate", -8.4328, 900'000)};

    if (!Check(
            !duplicateSource.Build(
                config, 1'000'000, duplicates, sentinel, error),
            "DUPLICATE_VEHICLE_ID_REJECTED"))
    {
        return 1;
    }

    if (!Check(
            sentinel.station.stationId == 77 &&
            sentinel.referenceTimeItsMs == 123,
            "FAILED_BUILD_PRESERVES_OUTPUT"))
    {
        return 1;
    }

    CpmVehiclePerceptionConfig invalidConfig = config;
    invalidConfig.stationLatitudeDeg = 91.0;

    CpmVehiclePerceptionSource invalidSource;
    CpmInput invalidOutput{};
    invalidOutput.station.stationId = 55;
    error.clear();

    if (!Check(
            !invalidSource.Build(
                invalidConfig, 1'000'000, {}, invalidOutput, error),
            "INVALID_RSU_GEOGRAPHY_REJECTED"))
    {
        return 1;
    }

    if (!Check(
            invalidOutput.station.stationId == 55,
            "INVALID_CONFIG_PRESERVES_OUTPUT"))
    {
        return 1;
    }

    std::cout << "CPM_VEHICLE_PERCEPTION_SOURCE=PASS\n";
    return 0;
}
