#include "vanetza_integration/sumo_adapter/sumo_vehicle_datagram.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << label << '\n';
    return condition;
}

bool Near(double actual, double expected)
{
    return std::isfinite(actual) &&
           std::abs(actual - expected) < 1e-9;
}

} // namespace

int main()
{
    bool passed = true;

    // Compatibilidade: os seis campos legados continuam válidos.
    const auto legacy =
        ns3::ParseSumoVehicleDatagram(
            "veh0,10.125,20.250,13.5,90.0,0.25");

    passed = Check(
        legacy.has_value() &&
        legacy->vehId == "veh0" &&
        Near(legacy->x, 10.125) &&
        Near(legacy->y, 20.250) &&
        Near(legacy->speed, 13.5) &&
        Near(legacy->heading, 90.0) &&
        Near(legacy->accel, 0.25) &&
        !legacy->simulationTimeUs.has_value(),
        "LEGACY_SIX_FIELD_MESSAGE_ACCEPTED") && passed;

    // O sétimo campo é tempo lógico SUMO em microssegundos.
    const auto timed =
        ns3::ParseSumoVehicleDatagram(
            "veh0,10,20,13,90,0.25,1234567");

    passed = Check(
        timed.has_value() &&
        timed->simulationTimeUs.has_value() &&
        *timed->simulationTimeUs == UINT64_C(1234567),
        "OPTIONAL_SIMULATION_TIMESTAMP_ACCEPTED") && passed;

    // Formato geográfico:
    // vehId,x,y,speed,heading,accel,timeUs,longitudeDeg,latitudeDeg
    const auto geographic =
        ns3::ParseSumoVehicleDatagram(
            "veh0,20489.043,5179.750,13,90,0.25,1234567,-8.4330521,41.3851057");

    passed = Check(
        geographic.has_value() &&
        geographic->simulationTimeUs.has_value() &&
        *geographic->simulationTimeUs == UINT64_C(1234567) &&
        geographic->longitudeDeg.has_value() &&
        Near(*geographic->longitudeDeg, -8.4330521) &&
        geographic->latitudeDeg.has_value() &&
        Near(*geographic->latitudeDeg, 41.3851057),
        "NINE_FIELD_GEODETIC_MESSAGE_ACCEPTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,10,180.0001,41"),
        "LONGITUDE_OUT_OF_RANGE_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,10,-8,90.0001"),
        "LATITUDE_OUT_OF_RANGE_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,10,nan,41"),
        "NON_FINITE_LONGITUDE_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,10,-8,nan"),
        "NON_FINITE_LATITUDE_REJECTED") && passed;

    const auto zeroTime =
        ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,0");

    passed = Check(
        zeroTime.has_value() &&
        zeroTime->simulationTimeUs.has_value() &&
        *zeroTime->simulationTimeUs == 0,
        "ZERO_SIMULATION_TIMESTAMP_ACCEPTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,-1"),
        "NEGATIVE_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,1.5"),
        "FRACTIONAL_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,not-a-time"),
        "NON_NUMERIC_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,"),
        "EMPTY_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4,5,10,unexpected"),
        "UNEXPECTED_EXTRA_FIELD_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1,2,3,4"),
        "INCOMPLETE_LEGACY_MESSAGE_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,nan,2,3,4,5"),
        "NON_FINITE_POSITION_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            "veh0,1x,2,3,4,5"),
        "NUMERIC_TRAILING_CHARACTERS_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoVehicleDatagram(
            ",1,2,3,4,5"),
        "EMPTY_VEHICLE_ID_REJECTED") && passed;

    std::cout << "SUMO_VEHICLE_DATAGRAM_CONTRACT="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
