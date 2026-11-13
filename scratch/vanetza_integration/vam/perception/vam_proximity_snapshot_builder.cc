#include "vanetza_integration/vam/perception/vam_proximity_snapshot_builder.h"

#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace vanetza_integration
{
namespace vam
{
namespace
{

constexpr double kTraCIUnavailable = -1073741824.0;

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}

bool IsAvailableValue(double value)
{
    return std::isfinite(value) && value != kTraCIUnavailable;
}

bool IsValidThresholds(const VamProximityThresholds& thresholds)
{
    return std::isfinite(thresholds.minimumSafeLateralDistanceMeters) &&
           std::isfinite(thresholds.minimumSafeLongitudinalDistanceMeters) &&
           std::isfinite(thresholds.minimumSafeVerticalDistanceMeters) &&
           thresholds.minimumSafeLateralDistanceMeters > 0.0 &&
           thresholds.minimumSafeLongitudinalDistanceMeters > 0.0 &&
           thresholds.minimumSafeVerticalDistanceMeters > 0.0;
}

bool IsValidPersonState(const ns3::SumoPersonState& person)
{
    const auto& sample = person.sample;

    return person.firstObservedAtUs <= person.simulationTimeUs &&
           IsAvailableValue(sample.x) &&
           IsAvailableValue(sample.y) &&
           IsAvailableValue(sample.speed) &&
           sample.speed >= 0.0 &&
           IsAvailableValue(sample.angleDeg);
}

bool IsValidVehicleState(
    const ns3::VehicleState& vehicle,
    std::uint64_t presenceEpochUs)
{
    if (!vehicle.sumoSimulationTimeUs.has_value() ||
        presenceEpochUs > *vehicle.sumoSimulationTimeUs)
    {
        return false;
    }

    const auto& position = vehicle.mobility.position;

    return IsAvailableValue(position.x) &&
           IsAvailableValue(position.y) &&
           std::isfinite(position.z) &&
           IsAvailableValue(vehicle.mobility.heading);
}

VamProximityKinematics PersonKinematics(
    const ns3::SumoPersonState& person)
{
    return {
        person.sample.x,
        person.sample.y,
        0.0, // SUMO Person datagram does not carry altitude.
        person.sample.angleDeg,
        person.simulationTimeUs
    };
}

VamProximityKinematics VehicleKinematics(
    const ns3::VehicleState& vehicle)
{
    const auto& position = vehicle.mobility.position;

    return {
        position.x,
        position.y,
        position.z,
        vehicle.mobility.heading,
        *vehicle.sumoSimulationTimeUs
    };
}

} // namespace

bool
VamProximitySnapshotBuilder::BuildForEgo(
    const std::string& egoPersonId,
    const ns3::SumoPersonRegistry& persons,
    const ns3::VehicleRegistry& vehicles,
    const ns3::SumoVehicleLifecycleRegistry& vehicleLifecycle,
    const VamProximitySnapshotBuilderConfig& config,
    VamProximitySnapshotSet& output,
    std::string& error)
{
    if (egoPersonId.empty())
    {
        return Reject(error, "Ego person ID must not be empty");
    }

    if (!IsValidThresholds(config.vehicleThresholds) ||
        !IsValidThresholds(config.vruThresholds))
    {
        return Reject(
            error,
            "Vehicle and VRU proximity thresholds must be finite and positive");
    }

    const auto& activePersons = persons.GetActivePersons();
    const auto egoIt = activePersons.find(egoPersonId);

    if (egoIt == activePersons.end())
    {
        return Reject(error, "Ego person is not active in SUMO registry");
    }

    const auto& egoState = egoIt->second;
    if (!IsValidPersonState(egoState))
    {
        return Reject(error, "Ego person has invalid or inconsistent SUMO state");
    }

    VamProximitySnapshotSet candidate{};
    candidate.ego.personId = egoPersonId;
    candidate.ego.presenceEpochUs = egoState.firstObservedAtUs;
    candidate.ego.kinematics = PersonKinematics(egoState);

    candidate.targets.reserve(activePersons.size() + vehicles.Size());

    // SUMO Person is the source-level participant category. This does not
    // infer Pedestrian, Bicyclist or another semantic VRU profile.
    for (const auto& [personId, personState] : activePersons)
    {
        if (personId == egoPersonId)
        {
            continue;
        }

        if (!IsValidPersonState(personState))
        {
            return Reject(
                error,
                "Active SUMO person has invalid or inconsistent state");
        }

        VamProximityTargetSnapshot target{};
        target.participantId = personId;
        target.kind = VamProximityTargetKind::Vru;
        target.presenceEpochUs = personState.firstObservedAtUs;
        target.kinematics = PersonKinematics(personState);
        target.thresholds = config.vruThresholds;

        candidate.targets.push_back(std::move(target));
    }

    // VehicleRegistry retains VehicleState beyond SUMO departure. A state
    // is a SUMO target only when its independent lifecycle record is active.
    for (const auto& [vehicleId, vehiclePtr] : vehicles.GetAllVehicles())
    {
        if (!vehiclePtr)
        {
            continue;
        }

        const auto lifecycleState = vehicleLifecycle.Find(vehicleId);

        if (!lifecycleState.has_value() || !lifecycleState->active)
        {
            continue;
        }

        // A legacy first UPSERT without a timestamp has no recoverable
        // presence epoch. Never infer it from a later mobility sample.
        if (!lifecycleState->firstObservedAtUs.has_value())
        {
            continue;
        }

        const auto& vehicle = *vehiclePtr;

        // An active lifecycle entry does not imply that mobility has already
        // been initialized or that a timestamped sample is available.
        if (!vehicle.initialized ||
            !vehicle.sumoSimulationTimeUs.has_value())
        {
            continue;
        }

        if (!IsValidVehicleState(
                vehicle, *lifecycleState->firstObservedAtUs))
        {
            return Reject(
                error,
                "Active SUMO vehicle has invalid or inconsistent mobility state");
        }

        VamProximityTargetSnapshot target{};
        target.participantId = vehicleId;
        target.kind = VamProximityTargetKind::Vehicle;
        target.presenceEpochUs = *lifecycleState->firstObservedAtUs;
        target.kinematics = VehicleKinematics(vehicle);
        target.thresholds = config.vehicleThresholds;

        candidate.targets.push_back(std::move(target));
    }

    // VehicleRegistry is an unordered map. Stable ordering makes snapshots
    // reproducible and avoids nondeterministic test/runtime observations.
    std::sort(
        candidate.targets.begin(),
        candidate.targets.end(),
        [](const auto& lhs, const auto& rhs)
        {
            if (lhs.kind != rhs.kind)
            {
                return lhs.kind < rhs.kind;
            }

            return lhs.participantId < rhs.participantId;
        });

    output = std::move(candidate);
    error.clear();
    return true;
}

} // namespace vam
} // namespace vanetza_integration
