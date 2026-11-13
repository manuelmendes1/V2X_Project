#include "vanetza_integration/vam/perception/vam_proximity_snapshot_builder.h"

#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

using namespace ns3;
using namespace vanetza_integration::vam;

namespace
{

bool Check(bool condition, const char* description)
{
    std::cout
        << (condition ? "[PASS] " : "[FAIL] ")
        << description
        << '\n';
    return condition;
}

SumoPersonDatagram PersonUpsert(
    const std::string& personId,
    std::uint64_t timestampUs,
    double x,
    double y,
    double headingDeg,
    std::optional<std::string> vClass = std::nullopt)
{
    SumoPersonDatagram datagram{};
    datagram.eventType = SumoPersonEventType::Upsert;
    datagram.personId = personId;
    datagram.sample = SumoPersonSample{
        x, y, 1.25, headingDeg};
    datagram.simulationTimeUs = timestampUs;
    datagram.sumoVClass = std::move(vClass);
    return datagram;
}

SumoPersonDatagram PersonRemove(
    const std::string& personId,
    std::uint64_t timestampUs)
{
    SumoPersonDatagram datagram{};
    datagram.eventType = SumoPersonEventType::Remove;
    datagram.personId = personId;
    datagram.simulationTimeUs = timestampUs;
    return datagram;
}

bool AddVehicle(
    VehicleRegistry& vehicles,
    SumoVehicleLifecycleRegistry& lifecycle,
    const std::string& vehicleId,
    std::optional<std::uint64_t> firstObservedAtUs,
    std::optional<std::uint64_t> sampleTimeUs,
    bool initialized,
    double x,
    double y,
    double z,
    double headingDeg,
    bool tracked = true)
{
    if (tracked)
    {
        if (!lifecycle.ApplyUpsert(vehicleId, firstObservedAtUs))
        {
            return false;
        }

        if (sampleTimeUs.has_value() &&
            !lifecycle.ApplyUpsert(vehicleId, sampleTimeUs))
        {
            return false;
        }
    }

    VehicleState* state = vehicles.CreateOrGet(0, vehicleId);
    if (state == nullptr)
    {
        return false;
    }

    state->initialized = initialized;
    state->sumoSimulationTimeUs = sampleTimeUs;
    state->mobility.position = Vector(x, y, z);
    state->mobility.heading = headingDeg;
    return true;
}

const VamProximityTargetSnapshot*
FindTarget(
    const VamProximitySnapshotSet& snapshots,
    VamProximityTargetKind kind,
    const std::string& participantId)
{
    for (const auto& target : snapshots.targets)
    {
        if (target.kind == kind &&
            target.participantId == participantId)
        {
            return &target;
        }
    }

    return nullptr;
}

bool Near(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= 1.0e-9;
}

} // namespace

int main()
{
    auto& vehicles = VehicleRegistry::Instance();
    vehicles.Clear();

    SumoVehicleLifecycleRegistry lifecycle;
    SumoPersonRegistry persons;

    bool ok = true;

    // The ego's first observation must survive subsequent accepted updates.
    ok &= Check(
        persons.Apply(PersonUpsert(
            "ego-person", 1000, 0.0, 0.0, 90.0, "pedestrian")),
        "fixture accepts first ego-person observation");

    ok &= Check(
        persons.Apply(PersonUpsert(
            "ego-person", 2000, 0.5, 0.0, 90.0, "pedestrian")),
        "fixture accepts latest ego-person sample");

    // Another SUMO person with an unclassified vClass remains a source-level
    // Person target. The builder must not infer its semantic VRU profile.
    ok &= Check(
        persons.Apply(PersonUpsert(
            "person-target", 1100, 4.5, 2.0, 180.0, "bicycle")),
        "fixture accepts another active person");

    ok &= Check(
        persons.Apply(PersonUpsert(
            "person-target", 2100, 5.0, 2.0, 180.0, "bicycle")),
        "fixture accepts latest target-person sample");

    ok &= Check(
        persons.Apply(PersonUpsert(
            "person-departed", 1200, 2.0, 2.0, 90.0, "pedestrian")),
        "fixture accepts person that will depart");

    ok &= Check(
        persons.Apply(PersonRemove("person-departed", 2200)),
        "fixture accepts person REMOVE");

    // Active vehicle: lifecycle first-observation epoch differs from the
    // latest mobility sample timestamp.
    ok &= Check(
        AddVehicle(
            vehicles, lifecycle, "vehicle-active",
            UINT64_C(1200), UINT64_C(2150), true,
            8.0, 9.0, 1.5, 270.0),
        "fixture creates initialized active SUMO vehicle");

    // Removed VehicleState remains owned by VehicleRegistry. Its lifecycle
    // tombstone must nevertheless exclude it from the snapshot.
    ok &= Check(
        AddVehicle(
            vehicles, lifecycle, "vehicle-departed",
            UINT64_C(1300), UINT64_C(2250), true,
            12.0, 9.0, 0.0, 90.0),
        "fixture creates vehicle that will depart");

    ok &= Check(
        lifecycle.ApplyRemove("vehicle-departed", UINT64_C(2300)),
        "fixture accepts vehicle REMOVE");

    // Timestamp-less first observation must remain unknown even after a
    // later timestamped sample.
    ok &= Check(
        AddVehicle(
            vehicles, lifecycle, "vehicle-legacy",
            std::nullopt, UINT64_C(2150), true,
            15.0, 9.0, 0.0, 90.0),
        "fixture creates active vehicle with unknown first-observation time");

    // An initialized-looking untracked state is not evidence of SUMO
    // lifecycle presence and must not be silently promoted into the target set.
    ok &= Check(
        AddVehicle(
            vehicles, lifecycle, "vehicle-untracked",
            std::nullopt, UINT64_C(2150), true,
            18.0, 9.0, 0.0, 90.0, false),
        "fixture creates untracked vehicle state");

    // The lifecycle says present, but VehicleState has not received an
    // accepted mobility sample yet.
    ok &= Check(
        AddVehicle(
            vehicles, lifecycle, "vehicle-uninitialized",
            UINT64_C(1400), std::nullopt, false,
            20.0, 9.0, 0.0, 90.0),
        "fixture creates active but uninitialized vehicle state");

    VamProximitySnapshotBuilderConfig config{};
    config.vehicleThresholds = {
        11.0, 12.0, 5.0};
    config.vruThresholds = {
        7.0, 8.0, 3.0};

    VamProximitySnapshotSet snapshots{};
    std::string error;

    const bool built = VamProximitySnapshotBuilder::BuildForEgo(
        "ego-person",
        persons,
        vehicles,
        lifecycle,
        config,
        snapshots,
        error);

    ok &= Check(
        built,
        "fixture snapshot build succeeds");

    if (!built)
    {
        std::cerr << "SNAPSHOT_BUILD_ERROR=" << error << '\n';
        vehicles.Clear();
        return 1;
    }

    ok &= Check(
        snapshots.ego.personId == "ego-person" &&
            snapshots.ego.presenceEpochUs == UINT64_C(1000) &&
            snapshots.ego.kinematics.sampleTimeUs == UINT64_C(2000) &&
            Near(snapshots.ego.kinematics.xMeters, 0.5) &&
            Near(snapshots.ego.kinematics.yMeters, 0.0) &&
            Near(snapshots.ego.kinematics.zMeters, 0.0) &&
            Near(snapshots.ego.kinematics.headingDeg, 90.0),
        "EGO_SNAPSHOT_USES_PERSON_REGISTRY_HISTORY");

    const auto* activeVehicle = FindTarget(
        snapshots,
        VamProximityTargetKind::Vehicle,
        "vehicle-active");

    ok &= Check(
        activeVehicle != nullptr &&
            activeVehicle->presenceEpochUs == UINT64_C(1200) &&
            activeVehicle->kinematics.sampleTimeUs == UINT64_C(2150) &&
            Near(activeVehicle->kinematics.xMeters, 8.0) &&
            Near(activeVehicle->kinematics.yMeters, 9.0) &&
            Near(activeVehicle->kinematics.zMeters, 1.5) &&
            Near(activeVehicle->kinematics.headingDeg, 270.0),
        "ACTIVE_VEHICLE_USES_LIFECYCLE_PRESENCE_EPOCH");

    const auto* activePerson = FindTarget(
        snapshots,
        VamProximityTargetKind::Vru,
        "person-target");

    ok &= Check(
        activePerson != nullptr &&
            activePerson->presenceEpochUs == UINT64_C(1100) &&
            activePerson->kinematics.sampleTimeUs == UINT64_C(2100) &&
            Near(activePerson->kinematics.xMeters, 5.0) &&
            Near(activePerson->kinematics.yMeters, 2.0) &&
            Near(activePerson->kinematics.zMeters, 0.0) &&
            Near(activePerson->kinematics.headingDeg, 180.0),
        "ACTIVE_PERSON_IS_A_VRU_TARGET_WITHOUT_PROFILE_INFERENCE");

    ok &= Check(
        FindTarget(
            snapshots,
            VamProximityTargetKind::Vehicle,
            "vehicle-departed") == nullptr,
        "REMOVED_VEHICLE_IS_EXCLUDED");

    ok &= Check(
        FindTarget(
            snapshots,
            VamProximityTargetKind::Vru,
            "person-departed") == nullptr,
        "REMOVED_PERSON_IS_EXCLUDED");

    ok &= Check(
        FindTarget(
            snapshots,
            VamProximityTargetKind::Vehicle,
            "vehicle-legacy") == nullptr,
        "VEHICLE_WITH_UNKNOWN_PRESENCE_EPOCH_IS_EXCLUDED");

    ok &= Check(
        FindTarget(
            snapshots,
            VamProximityTargetKind::Vehicle,
            "vehicle-untracked") == nullptr,
        "UNTRACKED_VEHICLE_IS_NOT_ASSUMED_SUMO_PRESENT");

    ok &= Check(
        FindTarget(
            snapshots,
            VamProximityTargetKind::Vehicle,
            "vehicle-uninitialized") == nullptr,
        "UNINITIALIZED_VEHICLE_IS_EXCLUDED");

    ok &= Check(
        FindTarget(
            snapshots,
            VamProximityTargetKind::Vru,
            "ego-person") == nullptr,
        "EGO_PERSON_IS_NOT_INCLUDED_AS_TARGET");

    ok &= Check(
        snapshots.targets.size() == 2,
        "snapshot contains exactly one active vehicle and one other person");

    ok &= Check(
        activeVehicle != nullptr &&
            Near(
                activeVehicle->thresholds.minimumSafeLateralDistanceMeters,
                11.0) &&
            Near(
                activeVehicle->thresholds.minimumSafeLongitudinalDistanceMeters,
                12.0) &&
            Near(
                activeVehicle->thresholds.minimumSafeVerticalDistanceMeters,
                5.0) &&
            activePerson != nullptr &&
            Near(
                activePerson->thresholds.minimumSafeLateralDistanceMeters,
                7.0) &&
            Near(
                activePerson->thresholds.minimumSafeLongitudinalDistanceMeters,
                8.0) &&
            Near(
                activePerson->thresholds.minimumSafeVerticalDistanceMeters,
                3.0),
        "THRESHOLDS_ARE_SELECTED_BY_TARGET_KIND");

    // A failed build must not destroy a previously valid caller snapshot.
    VamProximitySnapshotSet previous = snapshots;
    error.clear();

    const bool missingEgoBuilt =
        VamProximitySnapshotBuilder::BuildForEgo(
            "missing-ego",
            persons,
            vehicles,
            lifecycle,
            config,
            snapshots,
            error);

    ok &= Check(
        !missingEgoBuilt &&
            !error.empty() &&
            snapshots.ego.personId == previous.ego.personId &&
            snapshots.ego.presenceEpochUs ==
                previous.ego.presenceEpochUs &&
            snapshots.targets.size() == previous.targets.size(),
        "FAILED_BUILD_PRESERVES_OUTPUT");

    vehicles.Clear();

    if (!ok)
    {
        std::cerr << "VAM_PROXIMITY_SNAPSHOT_BUILDER=FAIL\n";
        return 1;
    }

    std::cout << "VAM_PROXIMITY_SNAPSHOT_BUILDER=PASS\n";
    return 0;
}
