#include "vanetza_integration/sumo_adapter/sumo_vehicle_datagram.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

using namespace ns3;

namespace
{
bool Check(bool condition, const char* description)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << description << '\n';
    return condition;
}
}

int main()
{
    bool ok = true;

    const auto valid =
        ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,1234567");

    ok &= Check(
        valid.has_value() &&
            valid->vehId == "veh-1" &&
            valid->simulationTimeUs == UINT64_C(1234567),
        "valid vehicle REMOVE datagram is parsed");

    const auto zeroTime =
        ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,0");

    ok &= Check(
        zeroTime.has_value() &&
            zeroTime->simulationTimeUs == UINT64_C(0),
        "zero logical timestamp is accepted");

    const auto maxTime =
        ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1," +
            std::to_string(std::numeric_limits<std::uint64_t>::max()));

    ok &= Check(
        maxTime.has_value() &&
            maxTime->simulationTimeUs ==
                std::numeric_limits<std::uint64_t>::max(),
        "maximum uint64 timestamp is accepted");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,,100"),
        "empty vehicle ID is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1"),
        "missing timestamp is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,"),
        "empty timestamp is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,-1"),
        "negative timestamp is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,1.5"),
        "fractional timestamp is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,not-a-time"),
        "non-numeric timestamp is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,18446744073709551616"),
        "uint64 overflow is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_PERSON,REMOVE,veh-1,100"),
        "wrong event namespace is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,REMOVE,veh-1,100,extra"),
        "unexpected extra field is rejected");

    ok &= Check(
        !ParseSumoVehicleRemoveDatagram(
            "SUMO_VEHICLE,UPSERT,veh-1,100"),
        "wrong vehicle event type is rejected");

    std::cout << "SUMO_VEHICLE_REMOVE_DATAGRAM="
              << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? 0 : 1;
}
