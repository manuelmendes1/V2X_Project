#include "vanetza_integration/sumo_adapter/sumo_person_datagram.h"

#include <cmath>
#include <cstdint>
#include <iostream>

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

    const auto upsert = ns3::ParseSumoPersonDatagram(
        "SUMO_PERSON,UPSERT,pedestrian-17,10.125,20.250,1.400,90.000,1234567");

    passed = Check(
        upsert.has_value() &&
        upsert->eventType == ns3::SumoPersonEventType::Upsert &&
        upsert->personId == "pedestrian-17" &&
        upsert->simulationTimeUs == UINT64_C(1234567) &&
        upsert->sample.has_value() &&
        Near(upsert->sample->x, 10.125) &&
        Near(upsert->sample->y, 20.250) &&
        Near(upsert->sample->speed, 1.400) &&
        Near(upsert->sample->angleDeg, 90.000),
        "UPSERT_PERSON_SAMPLE_PARSED") && passed;

    const auto remove = ns3::ParseSumoPersonDatagram(
        "SUMO_PERSON,REMOVE,pedestrian-17,1234568");

    passed = Check(
        remove.has_value() &&
        remove->eventType == ns3::SumoPersonEventType::Remove &&
        remove->personId == "pedestrian-17" &&
        remove->simulationTimeUs == UINT64_C(1234568) &&
        !remove->sample.has_value(),
        "REMOVE_EVENT_HAS_NO_MOBILITY_SAMPLE") && passed;

    const auto zeroTime = ns3::ParseSumoPersonDatagram(
        "SUMO_PERSON,UPSERT,ped0,0,0,0,0,0");
    passed = Check(
        zeroTime.has_value() &&
        zeroTime->simulationTimeUs == 0,
        "ZERO_SIMULATION_TIMESTAMP_ACCEPTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "pedestrian-17,10.125,20.250,1.400,90.000,1234567"),
        "UNDISCRIMINATED_VEHICLE_SHAPED_MESSAGE_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,3,4"),
        "UPSERT_WITHOUT_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,3,4,5,extra,overflow"),
        "UPSERT_TENTH_FIELD_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,REMOVE,ped0,10,extra"),
        "REMOVE_EXTRA_FIELD_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UNKNOWN,ped0,10"),
        "UNKNOWN_PERSON_EVENT_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_OTHER,UPSERT,ped0,1,2,3,4,5"),
        "UNKNOWN_DISCRIMINATOR_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,,1,2,3,4,5"),
        "EMPTY_PERSON_ID_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,nan,2,3,4,5"),
        "NON_FINITE_X_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,inf,3,4,5"),
        "NON_FINITE_Y_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,-0.1,4,5"),
        "NEGATIVE_SPEED_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,1,nan,5"),
        "NON_FINITE_ANGLE_REJECTED") && passed;

    // TraCI uses -2^30 as its unavailable-value sentinel for Person data.
    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,-1073741824,-1073741824,1,90,5"),
        "TRACI_UNAVAILABLE_POSITION_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,-1073741824,90,5"),
        "TRACI_UNAVAILABLE_SPEED_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,1,-1073741824,5"),
        "TRACI_UNAVAILABLE_ANGLE_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,1,90,-1"),
        "NEGATIVE_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,1,90,1.5"),
        "FRACTIONAL_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,UPSERT,ped0,1,2,1,90,"),
        "EMPTY_TIMESTAMP_REJECTED") && passed;

    passed = Check(
        !ns3::ParseSumoPersonDatagram(
            "SUMO_PERSON,REMOVE,ped0,18446744073709551616"),
        "UINT64_TIMESTAMP_OVERFLOW_REJECTED") && passed;

    std::cout << "SUMO_PERSON_DATAGRAM_CONTRACT="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
