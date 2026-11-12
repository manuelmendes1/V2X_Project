#include "vanetza_integration/vam/perception/vam_proximity_provider.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace vanetza_integration::vam;

namespace
{

int failures = 0;

void Check(bool condition, const char* label)
{
    std::cout
        << (condition ? "PASS " : "FAIL ")
        << label << '\n';

    if (!condition)
    {
        ++failures;
    }
}

bool Near(double actual, double expected)
{
    return std::abs(actual - expected) < 1e-9;
}

VamProximityKinematics Kinematics(
    double x,
    double y,
    double z,
    double headingDeg,
    std::uint64_t sampleTimeUs)
{
    VamProximityKinematics value;
    value.xMeters = x;
    value.yMeters = y;
    value.zMeters = z;
    value.headingDeg = headingDeg;
    value.sampleTimeUs = sampleTimeUs;
    return value;
}

VamProximityEgoSnapshot Ego(
    const std::string& personId,
    std::uint64_t presenceEpochUs,
    std::uint64_t sampleTimeUs)
{
    VamProximityEgoSnapshot value;
    value.personId = personId;
    value.presenceEpochUs = presenceEpochUs;
    value.kinematics = Kinematics(
        0.0, 0.0, 0.0, 0.0, sampleTimeUs);
    return value;
}

VamProximityTargetSnapshot Vehicle(
    const std::string& vehicleId,
    std::uint64_t presenceEpochUs,
    std::uint64_t sampleTimeUs,
    double x,
    double y,
    double z,
    double headingDeg)
{
    VamProximityTargetSnapshot value;
    value.participantId = vehicleId;
    value.kind = VamProximityTargetKind::Vehicle;
    value.presenceEpochUs = presenceEpochUs;
    value.kinematics = Kinematics(
        x, y, z, headingDeg, sampleTimeUs);

    value.thresholds.minimumSafeLateralDistanceMeters = 10.0;
    value.thresholds.minimumSafeLongitudinalDistanceMeters = 12.0;
    value.thresholds.minimumSafeVerticalDistanceMeters = 5.0;

    return value;
}

VamProximityTargetSnapshot VruTarget(
    const std::string& personId,
    std::uint64_t presenceEpochUs,
    std::uint64_t sampleTimeUs,
    double x,
    double y,
    double headingDeg)
{
    VamProximityTargetSnapshot value;
    value.participantId = personId;
    value.kind = VamProximityTargetKind::Vru;
    value.presenceEpochUs = presenceEpochUs;
    value.kinematics = Kinematics(
        x, y, 0.0, headingDeg, sampleTimeUs);

    value.thresholds.minimumSafeLateralDistanceMeters = 10.0;
    value.thresholds.minimumSafeLongitudinalDistanceMeters = 12.0;
    value.thresholds.minimumSafeVerticalDistanceMeters = 5.0;

    return value;
}

} // namespace

int main()
{
    VamProximityProvider provider;

    VamProximityProviderConfig config;
    config.maximumSampleAgeUs = 200000;

    std::vector<VamSafeProximityObservation> output;
    std::string error;

    auto ego = Ego("ego-a", 10, 1000000);
    auto vehicle = Vehicle(
        "vehicle-1", 20, 1000000,
        3.0, 4.0, 2.0, 0.0);

    // 1. First detection, geometry and vertical separation.
    bool accepted = provider.BuildForEgo(
        ego, {vehicle}, 1000000, config, output, error);

    Check(
        accepted && error.empty() && output.size() == 1,
        "FIRST_VALID_TARGET_PRODUCES_ONE_OBSERVATION");

    if (output.size() == 1)
    {
        Check(
            output[0].firstObservedAtMs == 1000,
            "FIRST_OBSERVATION_USES_LOGICAL_REFERENCE_TIME_MS");

        Check(
            Near(output[0].lateralDistanceMeters, 3.0) &&
            Near(output[0].longitudinalDistanceMeters, 4.0) &&
            Near(output[0].verticalDistanceMeters, 2.0),
            "VEHICLE_GEOMETRY_USES_TARGET_HEADING_ZERO_DEGREES");

        Check(
            Near(output[0].minimumSafeLateralDistanceMeters, 10.0) &&
            Near(output[0].minimumSafeLongitudinalDistanceMeters, 12.0) &&
            Near(output[0].minimumSafeVerticalDistanceMeters, 5.0),
            "EXTERNALLY_SUPPLIED_THRESHOLDS_ARE_PRESERVED");
    }

    // 2. Movement does not reset first detection during the same presence.
    ego = Ego("ego-a", 10, 1100000);
    vehicle = Vehicle(
        "vehicle-1", 20, 1100000,
        5.0, 4.0, 2.0, 0.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {vehicle}, 1100000, config, output, error);

    Check(
        accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 1000 &&
            Near(output[0].lateralDistanceMeters, 5.0),
        "ACTIVE_PRESENCE_PRESERVES_FIRST_DETECTION_AND_UPDATES_DISTANCE");

    // 3. The same physical target has independent history per ego.
    auto secondEgo = Ego("ego-b", 30, 1200000);
    vehicle = Vehicle(
        "vehicle-1", 20, 1200000,
        3.0, 4.0, 2.0, 0.0);

    error.clear();
    accepted = provider.BuildForEgo(
        secondEgo, {vehicle}, 1200000, config, output, error);

    Check(
        accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 1200,
        "FIRST_DETECTION_HISTORY_IS_INDEPENDENT_PER_EGO");

    // 4. A new target presence epoch resets the pair history.
    ego = Ego("ego-a", 10, 1300000);
    vehicle = Vehicle(
        "vehicle-1", 40, 1300000,
        3.0, 4.0, 2.0, 0.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {vehicle}, 1300000, config, output, error);

    Check(
        accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 1300,
        "TARGET_REENTRY_RESETS_FIRST_DETECTION");

    // 5. A new ego presence epoch resets all its pair histories.
    ego = Ego("ego-a", 50, 1400000);
    vehicle = Vehicle(
        "vehicle-1", 40, 1400000,
        3.0, 4.0, 2.0, 0.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {vehicle}, 1400000, config, output, error);

    Check(
        accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 1400,
        "EGO_REENTRY_RESETS_PAIR_HISTORY");

    // 6. Stale target data produces no measurement but does not erase
    //    the history while the participant remains in the active snapshot.
    ego = Ego("ego-a", 50, 1600000);
    vehicle = Vehicle(
        "vehicle-1", 40, 1000000,
        3.0, 4.0, 2.0, 0.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {vehicle}, 1600000, config, output, error);

    Check(
        accepted && output.empty(),
        "STALE_TARGET_SAMPLE_PRODUCES_NO_OBSERVATION");

    ego = Ego("ego-a", 50, 1700000);
    vehicle = Vehicle(
        "vehicle-1", 40, 1700000,
        3.0, 4.0, 2.0, 0.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {vehicle}, 1700000, config, output, error);

    Check(
        accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 1400,
        "TEMPORARY_STALE_SAMPLE_PRESERVES_EXISTING_HISTORY");

    // 7. A person must not be detected as a target by itself.
    ego = Ego("ego-a", 50, 1800000);
    auto self = VruTarget(
        "ego-a", 50, 1800000, 1.0, 1.0, 90.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {self}, 1800000, config, output, error);

    Check(
        accepted && output.empty(),
        "EGO_PERSON_IS_EXCLUDED_FROM_ITS_OWN_TARGETS");

    // 8. For a VRU target, using the target heading is an explicit
    //    project geometry extension, not a normative vehicle definition.
    ego = Ego("ego-c", 60, 1900000);
    auto otherVru = VruTarget(
        "person-target", 70, 1900000,
        4.0, 3.0, 90.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {otherVru}, 1900000, config, output, error);

    Check(
        accepted && output.size() == 1 &&
            Near(output[0].lateralDistanceMeters, 3.0) &&
            Near(output[0].longitudinalDistanceMeters, 4.0),
        "VRU_TARGET_HEADING_USES_DOCUMENTED_PROJECT_EXTENSION");

    // 9. Omitting an active target closes its pair history.
    ego = Ego("ego-c", 60, 2000000);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {}, 2000000, config, output, error);

    Check(
        accepted && output.empty(),
        "ABSENT_TARGET_IS_REMOVED_FROM_CURRENT_PAIR_SET");

    ego = Ego("ego-c", 60, 2100000);
    otherVru = VruTarget(
        "person-target", 80, 2100000,
        4.0, 3.0, 90.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {otherVru}, 2100000, config, output, error);

    Check(
        accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 2100,
        "RETURNING_TARGET_STARTS_A_NEW_DETECTION_HISTORY");

    // 10. Removing an ego must allow the integration layer to release
    //     all of its pair histories, even if the same identity is later
    //     evaluated again with the same presence token.
    ego = Ego("ego-c", 60, 2200000);
    otherVru = VruTarget(
        "person-target", 80, 2200000,
        4.0, 3.0, 90.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {otherVru}, 2200000, config, output, error);

    const bool previousHistoryPreserved =
        accepted && output.size() == 1 &&
        output[0].firstObservedAtMs == 2100;

    provider.ForgetEgo("ego-c");

    ego = Ego("ego-c", 60, 2300000);
    otherVru = VruTarget(
        "person-target", 80, 2300000,
        4.0, 3.0, 90.0);

    error.clear();
    accepted = provider.BuildForEgo(
        ego, {otherVru}, 2300000, config, output, error);

    Check(
        previousHistoryPreserved &&
            accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 2300,
        "FORGET_EGO_RESETS_HISTORY");

    // Forgetting an unknown ego must be harmless.
    provider.ForgetEgo("unknown-ego");


    // 11. Vehicle heading at 90 degrees: longitudinal axis points East.
    VamProximityProvider geometryProvider;
    auto geometryEgo = Ego("ego-geometry", 100, 2400000);
    auto eastboundVehicle = Vehicle(
        "vehicle-eastbound", 200, 2400000,
        4.0, 3.0, 0.0, 90.0);

    error.clear();
    accepted = geometryProvider.BuildForEgo(
        geometryEgo, {eastboundVehicle}, 2400000,
        config, output, error);

    Check(
        accepted && output.size() == 1 &&
            Near(output[0].lateralDistanceMeters, 3.0) &&
            Near(output[0].longitudinalDistanceMeters, 4.0),
        "VEHICLE_HEADING_NINETY_DEGREES_USES_TARGET_REFERENCE_FRAME");

    // 12. Freshness boundary is inclusive; one microsecond beyond it
    //     is stale and must not erase an established pair history.
    VamProximityProvider freshnessProvider;
    VamProximityProviderConfig freshnessConfig;
    freshnessConfig.maximumSampleAgeUs = 200000;

    auto freshnessEgo = Ego("ego-freshness", 10, 1000000);
    auto freshnessTarget = Vehicle(
        "vehicle-freshness", 20, 1000000,
        3.0, 4.0, 0.0, 0.0);

    error.clear();
    accepted = freshnessProvider.BuildForEgo(
        freshnessEgo, {freshnessTarget}, 1200000,
        freshnessConfig, output, error);

    const bool exactAgeAccepted =
        accepted && output.size() == 1 &&
        output[0].firstObservedAtMs == 1200;

    freshnessEgo = Ego("ego-freshness", 10, 1200001);

    error.clear();
    accepted = freshnessProvider.BuildForEgo(
        freshnessEgo, {freshnessTarget}, 1200001,
        freshnessConfig, output, error);

    const bool oneMicrosecondTooOldRejected =
        accepted && output.empty();

    freshnessTarget = Vehicle(
        "vehicle-freshness", 20, 1200001,
        3.0, 4.0, 0.0, 0.0);

    error.clear();
    accepted = freshnessProvider.BuildForEgo(
        freshnessEgo, {freshnessTarget}, 1200001,
        freshnessConfig, output, error);

    Check(
        exactAgeAccepted &&
            oneMicrosecondTooOldRejected &&
            accepted && output.size() == 1 &&
            output[0].firstObservedAtMs == 1200,
        "FRESHNESS_BOUNDARY_IS_INCLUSIVE_AND_STALE_SAMPLE_PRESERVES_HISTORY");

    // 13. A failed build must not commit a partially processed target
    //     or overwrite the caller's existing output.
    VamProximityProvider atomicProvider;
    auto atomicEgo = Ego("ego-atomic", 10, 3000000);
    auto atomicTarget = Vehicle(
        "vehicle-atomic", 20, 3000000,
        3.0, 4.0, 0.0, 0.0);

    error.clear();
    accepted = atomicProvider.BuildForEgo(
        atomicEgo, {atomicTarget}, 3000000,
        config, output, error);

    const bool initialAtomicHistory =
        accepted && output.size() == 1 &&
        output[0].firstObservedAtMs == 3000;

    auto invalidTarget = VruTarget(
        "", 30, 3100000, 1.0, 1.0, 90.0);

    atomicEgo = Ego("ego-atomic", 10, 3100000);
    atomicTarget = Vehicle(
        "vehicle-atomic", 20, 3100000,
        4.0, 4.0, 0.0, 0.0);

    VamSafeProximityObservation sentinel{};
    sentinel.firstObservedAtMs = 777;
    output.assign(1, sentinel);

    error.clear();
    const bool invalidBuildAccepted = atomicProvider.BuildForEgo(
        atomicEgo, {atomicTarget, invalidTarget}, 3100000,
        config, output, error);

    const bool rejectedBuildPreservedOutput =
        !invalidBuildAccepted && !error.empty() &&
        output.size() == 1 &&
        output.front().firstObservedAtMs == 777;

    atomicEgo = Ego("ego-atomic", 10, 3200000);
    atomicTarget = Vehicle(
        "vehicle-atomic", 20, 3200000,
        5.0, 4.0, 0.0, 0.0);

    error.clear();
    accepted = atomicProvider.BuildForEgo(
        atomicEgo, {atomicTarget}, 3200000,
        config, output, error);

    Check(
        initialAtomicHistory &&
            rejectedBuildPreservedOutput &&
            accepted && output.size() == 1 &&
            output.front().firstObservedAtMs == 3000,
        "REJECTED_BUILD_DOES_NOT_PARTIALLY_MUTATE_OUTPUT_OR_HISTORY");

    // 14. Finite input coordinates can still overflow during subtraction.
    //     Such a projection must be rejected before committing history.
    VamProximityProvider numericProvider;
    const double largestFinite =
        std::numeric_limits<double>::max();

    auto numericEgo = Ego("ego-numeric", 10, 4000000);
    numericEgo.kinematics.xMeters = -largestFinite;

    auto numericTarget = Vehicle(
        "vehicle-numeric", 20, 4000000,
        largestFinite, 0.0, 0.0, 0.0);

    output.assign(1, sentinel);
    error.clear();

    const bool numericBuildAccepted = numericProvider.BuildForEgo(
        numericEgo, {numericTarget}, 4000000,
        config, output, error);

    Check(
        !numericBuildAccepted &&
            !error.empty() &&
            output.size() == 1 &&
            output.front().firstObservedAtMs == 777,
        "PROJECTED_DISTANCES_MUST_REMAIN_FINITE_AND_REJECT_ATOMICALLY");

    provider.Clear();

    provider.Clear();

    if (failures != 0)
    {
        std::cerr << "VAM_PROXIMITY_PROVIDER=FAIL\n";
        return 1;
    }

    std::cout << "VAM_PROXIMITY_PROVIDER=PASS\n";
    return 0;
}
