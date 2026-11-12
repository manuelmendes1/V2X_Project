#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>

using ns3::ItsStationIdentityAuthority;
using ns3::ItsStationIdentityOwner;
using ns3::ItsStationIdentityOwnerKind;

namespace
{

unsigned int failures = 0;

void Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    if (!condition)
    {
        ++failures;
    }
}

ItsStationIdentityOwner Vehicle(std::string logicalId)
{
    return {ItsStationIdentityOwnerKind::Vehicle, std::move(logicalId)};
}

ItsStationIdentityOwner ItsStation(std::string logicalId)
{
    return {ItsStationIdentityOwnerKind::ItsStation, std::move(logicalId)};
}

} // namespace

int main()
{
    auto& authority = ItsStationIdentityAuthority::Instance();
    authority.Clear();

    const auto spatem = ItsStation("spatem-rsu-0");
    const auto cpm = ItsStation("cpm-rsu-9001");
    const auto vehicle0 = Vehicle("veh-0");
    const auto vehicle1 = Vehicle("veh-1");

    std::cout << "===== 1. INVALID IDENTITY REJECTION =====\n";

    Check(
        !authority.Reserve(Vehicle(""), 100),
        "EMPTY_OWNER_REJECTED");

    Check(
        !authority.Reserve(vehicle0, 0),
        "ZERO_STATION_ID_REJECTED");

    Check(
        authority.Size() == 0,
        "INVALID_RESERVATIONS_DO_NOT_MUTATE_STATE");

    std::cout << "\n===== 2. EXPLICIT RESERVATIONS =====\n";

    Check(
        authority.Reserve(spatem, 9001),
        "EXPLICIT_STATION_ID_RESERVED");

    Check(
        authority.Reserve(spatem, 9001),
        "SAME_OWNER_SAME_ID_IS_IDEMPOTENT");

    Check(
        authority.FindStationId(spatem) ==
            std::optional<std::uint32_t>{9001},
        "OWNER_LOOKUP_RETURNS_RESERVED_ID");

    Check(
        !authority.Reserve(spatem, 9002),
        "SAME_OWNER_CANNOT_CHANGE_STATION_ID");

    Check(
        authority.FindStationId(spatem) ==
            std::optional<std::uint32_t>{9001},
        "REJECTED_OWNER_REMAP_PRESERVES_ORIGINAL_ID");

    std::cout << "\n===== 3. GLOBAL COLLISION REJECTION =====\n";

    Check(
        !authority.Reserve(cpm, 9001),
        "TWO_ITS_STATIONS_CANNOT_SHARE_STATION_ID");

    Check(
        !authority.Reserve(Vehicle("vehicle-collision"), 9001),
        "VEHICLE_AND_ITS_STATION_CANNOT_SHARE_STATION_ID");

    Check(
        !authority.Reserve(Vehicle("spatem-rsu-0"), 9001),
        "OWNER_KIND_IS_PART_OF_LOGICAL_IDENTITY");

    Check(
        authority.IsReservedBy(9001, spatem),
        "RESERVATION_OWNER_CAN_BE_VERIFIED");

    Check(
        !authority.IsReservedBy(9001, cpm),
        "OTHER_OWNER_CANNOT_CLAIM_RESERVED_ID");

    std::cout << "\n===== 4. NODE-DERIVED RESERVATIONS =====\n";

    std::uint32_t vehicle0Id = 777;

    Check(
        authority.ReserveNodeDerived(vehicle0, 0, vehicle0Id) &&
            vehicle0Id == 1,
        "NODE_ZERO_DERIVES_STATION_ID_ONE");

    std::uint32_t vehicle1Id = 777;

    Check(
        authority.ReserveNodeDerived(vehicle1, 1, vehicle1Id) &&
            vehicle1Id == 2,
        "NODE_ONE_DERIVES_STATION_ID_TWO");

    std::uint32_t repeatedId = 888;

    Check(
        authority.ReserveNodeDerived(vehicle0, 0, repeatedId) &&
            repeatedId == vehicle0Id,
        "SAME_OWNER_SAME_NODE_DERIVATION_IS_IDEMPOTENT");

    std::uint32_t changedNodeId = 888;

    Check(
        !authority.ReserveNodeDerived(vehicle0, 10, changedNodeId) &&
            changedNodeId == 888,
        "DERIVED_OWNER_REMAP_IS_REJECTED_ATOMICALLY");

    std::uint32_t collisionOutput = 555;

    Check(
        !authority.ReserveNodeDerived(
            Vehicle("vehicle-rsu-collision"), 9000, collisionOutput) &&
            collisionOutput == 555,
        "NODE_DERIVED_ID_COLLISION_WITH_PREVIOUS_RS​​U_RESERVATION_REJECTED");

    std::uint32_t overflowOutput = 444;

    Check(
        !authority.ReserveNodeDerived(
            Vehicle("vehicle-overflow"),
            std::numeric_limits<std::uint32_t>::max(),
            overflowOutput) &&
            overflowOutput == 444,
        "NODE_ID_OVERFLOW_REJECTED_WITHOUT_OUTPUT_MUTATION");

    std::cout << "\n===== 5. RELEASE OWNERSHIP CHECKS =====\n";

    Check(
        !authority.Release(spatem, 9002),
        "WRONG_STATION_ID_CANNOT_RELEASE_RESERVATION");

    Check(
        !authority.Release(cpm, 9001),
        "WRONG_OWNER_CANNOT_RELEASE_RESERVATION");

    Check(
        authority.FindStationId(spatem) ==
            std::optional<std::uint32_t>{9001},
        "REJECTED_RELEASE_PRESERVES_RESERVATION");

    Check(
        authority.Release(spatem, 9001),
        "CORRECT_OWNER_CAN_RELEASE_RESERVATION");

    Check(
        !authority.FindStationId(spatem).has_value(),
        "RELEASE_REMOVES_OWNER_LOOKUP");

    Check(
        !authority.IsReservedBy(9001, spatem),
        "RELEASE_REMOVES_REVERSE_RESERVATION");

    Check(
        authority.Reserve(cpm, 9001),
        "RELEASED_ID_CAN_BE_RESERVED_BY_NEW_OWNER");

    std::cout << "\n===== 6. GLOBAL CLEAR =====\n";

    authority.Clear();

    Check(
        authority.Size() == 0,
        "CLEAR_REMOVES_ALL_RESERVATIONS");

    Check(
        !authority.FindStationId(cpm).has_value(),
        "CLEAR_REMOVES_OWNER_LOOKUPS");

    Check(
        authority.Reserve(cpm, 9001),
        "RESERVATION_CAN_BE_CREATED_AFTER_CLEAR");


    std::cout << "\n===== 7. SHARED AUTHORITY INSTANCE =====\n";

    auto& authorityA = ItsStationIdentityAuthority::Instance();
    auto& authorityB = ItsStationIdentityAuthority::Instance();

    authorityA.Clear();

    Check(
        &authorityA == &authorityB,
        "SHARED_AUTHORITY_INSTANCE_IS_SINGLETON");

    const auto sharedVehicle = Vehicle("shared-vehicle");
    const auto sharedStation = ItsStation("shared-rsu");

    Check(
        authorityA.Reserve(sharedVehicle, 7001),
        "SHARED_INSTANCE_ACCEPTS_FIRST_RESERVATION");

    Check(
        !authorityB.Reserve(sharedStation, 7001),
        "SHARED_INSTANCE_REJECTS_CROSS_COMPONENT_COLLISION");

    Check(
        authorityB.FindStationId(sharedVehicle) ==
            std::optional<std::uint32_t>{7001},
        "SHARED_INSTANCE_LOOKUP_SEES_OTHER_COMPONENT_RESERVATION");

    authorityA.Clear();

    std::cout << "\nITS_STATION_ID_AUTHORITY_FAILURES="
              << failures << '\n';

    if (failures != 0)
    {
        return 1;
    }

    std::cout << "ITS_STATION_ID_AUTHORITY=PASS\n";
    return 0;
}
