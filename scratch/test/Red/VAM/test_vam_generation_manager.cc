#include "vanetza_integration/vam/generation/vam_generation_manager.h"

#include <cmath>
#include <array>
#include <limits>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace vanetza_integration::vam;
using namespace vanetza_integration::vru::role;

namespace
{

int failures = 0;

void Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    if (!condition)
    {
        ++failures;
    }
}

VamInput PedestrianInput()
{
    VamInput input{};
    input.vruProfile = VruProfile::Pedestrian;
    input.vruRole = {VruRoleStatus::On, VbsState::ActiveStandalone};
    input.referencePosition.latitudeDeg = 0.0;
    input.referencePosition.longitudeDeg = 0.0;
    input.highFrequencyState.speedMps = 2.0;
    input.highFrequencyState.headingDeg = 90.0;
    return input;
}

VruRolePermissions TxAllowed()
{
    return {
        true,
        true,
        VruTxDecisionReason::ProfileAllowed,
        VruRxDecisionReason::RoleOn
    };
}

VruRolePermissions TxBlocked()
{
    return {
        false,
        true,
        VruTxDecisionReason::RoleOff,
        VruRxDecisionReason::RoleOn
    };
}

void SetEastOffsetMeters(VamInput& input, double meters)
{
    // At the equator, one degree of longitude is approximately R*pi/180 m.
    constexpr double kEarthRadiusMeters = 6371008.8;
    constexpr double kPi = 3.14159265358979323846;
    input.referencePosition.latitudeDeg = 0.0;
    input.referencePosition.longitudeDeg =
        meters * 180.0 / (kPi * kEarthRadiusMeters);
}

bool Evaluate(
    VamGenerationManager& manager,
    std::int64_t nowMs,
    const VamInput& input,
    const VamGenerationContext& context,
    VamGenerationResult& result,
    std::string& error,
    const VruRolePermissions& permissions = TxAllowed())
{
    return manager.Evaluate(
        nowMs, input, permissions, context, result, error);
}

bool Confirm(
    VamGenerationManager& manager,
    const VamGenerationResult& result,
    std::int64_t confirmedAtMs,
    std::string& error)
{
    return result.transactionId.has_value() &&
           manager.ConfirmGeneration(
               *result.transactionId, confirmedAtMs, error);
}

bool TestInitialGenerationAndMinimumInterval()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    const bool firstAccepted =
        Evaluate(manager, 0, input, {}, result, error);
    const bool firstCorrect =
        firstAccepted && error.empty() &&
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.shouldGenerate &&
        result.reason == VamGenerationReason::InitialActivation &&
        result.includeLowFrequencyContainer &&
        result.effectiveTGenVamMs == 100 &&
        result.transactionId.has_value();
    Check(firstCorrect, "FIRST_VAM_AFTER_VBS_ACTIVATION_IS_IMMEDIATE_AND_HAS_LF");

    const bool confirmed = Confirm(manager, result, 0, error);
    Check(confirmed && error.empty(), "INITIAL_GENERATION_CAN_BE_CONFIRMED");

    error.clear();
    const bool earlyAccepted =
        Evaluate(manager, 99, input, {}, result, error);
    Check(
        earlyAccepted && result.outcome == VamGenerationOutcome::NotDue &&
            !result.shouldGenerate,
        "T_GEN_VAM_MIN_BLOCKS_GENERATION_BEFORE_100_MS");

    error.clear();
    const bool unchangedAccepted =
        Evaluate(manager, 100, input, {}, result, error);
    Check(
        unchangedAccepted &&
            result.outcome == VamGenerationOutcome::NoTrigger &&
            !result.shouldGenerate,
        "NO_CHANGE_DOES_NOT_CAUSE_A_PERIODIC_VAM");
    return failures == 0;
}

bool TestActivationTransitionTriggersImmediately()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    input.vruRole.vbs = VbsState::Idle;
    VamGenerationResult result{};
    std::string error;

    const bool idleAccepted =
        Evaluate(manager, 0, input, {}, result, error);
    const bool idleBlocked = idleAccepted &&
        result.outcome == VamGenerationOutcome::BlockedByPolicy &&
        !result.shouldGenerate;

    input.vruRole.vbs = VbsState::ActiveStandalone;
    error.clear();
    const bool activatedAccepted =
        Evaluate(manager, 500, input, {}, result, error);
    const bool immediateAfterActivation = activatedAccepted &&
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.reason == VamGenerationReason::InitialActivation &&
        result.shouldGenerate;

    Check(
        idleBlocked && immediateAfterActivation,
        "VBS_IDLE_TO_ACTIVE_STANDALONE_TRIGGERS_FIRST_VAM_IMMEDIATELY");
    return idleBlocked && immediateAfterActivation;
}

bool TestPositionThresholdIsStrict()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    SetEastOffsetMeters(input, 4.0);
    error.clear();
    ok = Evaluate(manager, 100, input, {}, result, error) && ok;
    const bool boundaryDoesNotTrigger =
        result.outcome == VamGenerationOutcome::NoTrigger;

    SetEastOffsetMeters(input, 4.01);
    error.clear();
    ok = Evaluate(manager, 200, input, {}, result, error) && ok;
    Check(
        ok && boundaryDoesNotTrigger &&
            result.outcome == VamGenerationOutcome::GenerationCandidate &&
            result.reason == VamGenerationReason::PositionChanged,
        "POSITION_TRIGGER_REQUIRES_DISTANCE_STRICTLY_GREATER_THAN_4M");
    return ok && boundaryDoesNotTrigger &&
           result.outcome == VamGenerationOutcome::GenerationCandidate;
}

bool TestSpeedThresholdIsStrict()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;
    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    input.highFrequencyState.speedMps = 2.5;
    error.clear();
    ok = Evaluate(manager, 100, input, {}, result, error) && ok;
    const bool boundaryDoesNotTrigger =
        result.outcome == VamGenerationOutcome::NoTrigger;

    input.highFrequencyState.speedMps = 2.5001;
    error.clear();
    ok = Evaluate(manager, 200, input, {}, result, error) && ok;
    Check(
        ok && boundaryDoesNotTrigger &&
            result.outcome == VamGenerationOutcome::GenerationCandidate &&
            result.reason == VamGenerationReason::GroundSpeedChanged,
        "SPEED_TRIGGER_REQUIRES_DIFFERENCE_STRICTLY_GREATER_THAN_0_5_MPS");
    return ok && boundaryDoesNotTrigger &&
           result.outcome == VamGenerationOutcome::GenerationCandidate;
}

bool TestOrientationWrapAndStrictThreshold()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    input.highFrequencyState.headingDeg = 359.0;
    VamGenerationResult result{};
    std::string error;
    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    input.highFrequencyState.headingDeg = 3.0;
    error.clear();
    ok = Evaluate(manager, 100, input, {}, result, error) && ok;
    const bool boundaryDoesNotTrigger =
        result.outcome == VamGenerationOutcome::NoTrigger;

    input.highFrequencyState.headingDeg = 3.01;
    error.clear();
    ok = Evaluate(manager, 200, input, {}, result, error) && ok;
    Check(
        ok && boundaryDoesNotTrigger &&
            result.outcome == VamGenerationOutcome::GenerationCandidate &&
            result.reason ==
                VamGenerationReason::GroundVelocityOrientationChanged,
        "ORIENTATION_TRIGGER_USES_CIRCULAR_DIFFERENCE_AND_STRICT_4_DEGREES");
    return ok && boundaryDoesNotTrigger &&
           result.outcome == VamGenerationOutcome::GenerationCandidate;
}

bool TestMaximumGenerationIntervalIsStrict()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;
    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    error.clear();
    ok = Evaluate(manager, 5000, input, {}, result, error) && ok;
    const bool exactBoundaryDoesNotTrigger =
        result.outcome == VamGenerationOutcome::NoTrigger;

    error.clear();
    ok = Evaluate(manager, 5001, input, {}, result, error) && ok;
    Check(
        ok && exactBoundaryDoesNotTrigger &&
            result.outcome == VamGenerationOutcome::GenerationCandidate &&
            result.reason == VamGenerationReason::MaximumGenerationInterval,
        "MAX_INTERVAL_TRIGGER_OCCURS_ONLY_AFTER_5000_MS");
    return ok && exactBoundaryDoesNotTrigger &&
           result.outcome == VamGenerationOutcome::GenerationCandidate;
}

bool TestTrajectoryProbabilityUsesPercentagePoints()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationContext context{};
    VamGenerationResult result{};
    std::string error;

    context.trajectoryInterceptionProbabilityPercent = 20.0;
    bool ok = Evaluate(manager, 0, input, context, result, error) &&
              Confirm(manager, result, 0, error);

    context.trajectoryInterceptionProbabilityPercent = 30.0;
    error.clear();
    ok = Evaluate(manager, 100, input, context, result, error) && ok;
    const bool boundaryDoesNotTrigger =
        result.outcome == VamGenerationOutcome::NoTrigger;

    context.trajectoryInterceptionProbabilityPercent = 30.01;
    error.clear();
    ok = Evaluate(manager, 200, input, context, result, error) && ok;
    Check(
        ok && boundaryDoesNotTrigger &&
            result.outcome == VamGenerationOutcome::GenerationCandidate &&
            result.reason ==
                VamGenerationReason::TrajectoryInterceptionProbabilityChanged,
        "TRAJECTORY_TRIGGER_USES_MORE_THAN_10_PERCENTAGE_POINTS");
    return ok && boundaryDoesNotTrigger &&
           result.outcome == VamGenerationOutcome::GenerationCandidate;
}

bool TestLowFrequencyContainerPeriodicity()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;
    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              result.includeLowFrequencyContainer &&
              Confirm(manager, result, 0, error);

    SetEastOffsetMeters(input, 5.0);
    error.clear();
    ok = Evaluate(manager, 100, input, {}, result, error) && ok;
    const bool noEarlyLf = !result.includeLowFrequencyContainer;
    ok = Confirm(manager, result, 100, error) && ok;

    SetEastOffsetMeters(input, 10.0);
    error.clear();
    ok = Evaluate(manager, 2000, input, {}, result, error) && ok;
    Check(
        ok && noEarlyLf &&
            result.outcome == VamGenerationOutcome::GenerationCandidate &&
            result.includeLowFrequencyContainer,
        "LOW_FREQUENCY_CONTAINER_REPEATS_AFTER_2000_MS");
    return ok && noEarlyLf && result.includeLowFrequencyContainer;
}

bool TestDccIntervalClamping()
{
    const auto checkInterval = [](std::optional<std::uint32_t> dcc,
                                  std::uint32_t expected) {
        VamGenerationManager manager;
        VamGenerationContext context{};
        context.dccTGenVamMs = dcc;
        VamGenerationResult result{};
        std::string error;
        const bool accepted = Evaluate(
            manager, 0, PedestrianInput(), context, result, error);
        return accepted && error.empty() &&
               result.effectiveTGenVamMs == expected;
    };

    const bool missingUsesMin = checkInterval(std::nullopt, 100);
    const bool belowMinClamps = checkInterval(50, 100);
    const bool inRangePreserved = checkInterval(250, 250);
    const bool aboveMaxClamps = checkInterval(6000, 5000);
    Check(
        missingUsesMin && belowMinClamps && inRangePreserved && aboveMaxClamps,
        "DCC_T_GEN_VAM_IS_CLAMPED_TO_100_5000_MS");
    return missingUsesMin && belowMinClamps && inRangePreserved && aboveMaxClamps;
}

bool TestRoleAndVbsPolicyGate()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    bool ok = Evaluate(
        manager, 0, input, {}, result, error, TxBlocked());
    const bool permissionBlocks =
        ok && result.outcome == VamGenerationOutcome::BlockedByPolicy &&
        !result.shouldGenerate;

    input.vruRole.vbs = VbsState::Passive;
    error.clear();
    ok = Evaluate(manager, 100, input, {}, result, error) && ok;
    const bool passiveBlocks =
        result.outcome == VamGenerationOutcome::BlockedByPolicy &&
        !result.shouldGenerate;

    input.vruRole.vbs = VbsState::ActiveClusterLeader;
    error.clear();
    ok = Evaluate(manager, 200, input, {}, result, error) && ok;
    const bool leaderIsNotTreatedAsIndividual =
        result.outcome == VamGenerationOutcome::BlockedByPolicy &&
        !result.shouldGenerate;

    Check(
        ok && permissionBlocks && passiveBlocks &&
            leaderIsNotTreatedAsIndividual,
        "ROLE_AND_NON_STANDALONE_VBS_STATES_BLOCK_INDIVIDUAL_GENERATION");
    return ok && permissionBlocks && passiveBlocks &&
           leaderIsNotTreatedAsIndividual;
}

bool TestRedundancyPeerRequiresAllSimilarityConditions()
{
    const auto runCase = [](double positionOffsetMeters,
                            double speedDelta,
                            double headingDelta) {
        VamGenerationManager manager;
        auto input = PedestrianInput();
        VamGenerationResult result{};
        std::string error;
        bool ok = Evaluate(manager, 0, input, {}, result, error) &&
                  Confirm(manager, result, 0, error);

        SetEastOffsetMeters(input, 5.0);
        input.highFrequencyState.speedMps = 2.0;
        input.highFrequencyState.headingDeg = 90.0;
        VamGenerationContext context{};
        VamPeerObservation peer{};
        peer.referencePosition.latitudeDeg = 0.0;
        peer.referencePosition.longitudeDeg =
            input.referencePosition.longitudeDeg;
        peer.highFrequencyState.speedMps = 2.0;
        peer.highFrequencyState.headingDeg = 90.0;
        SetEastOffsetMeters(input, 5.0 + positionOffsetMeters);
        peer.referencePosition.longitudeDeg =
            5.0 * 180.0 /
            (3.14159265358979323846 * 6371008.8);
        peer.highFrequencyState.speedMps = 2.0 + speedDelta;
        peer.highFrequencyState.headingDeg = 90.0 + headingDelta;
        context.redundancy.similarPeer = peer;

        error.clear();
        ok = Evaluate(manager, 100, input, context, result, error) && ok;
        return std::make_pair(ok, result.outcome);
    };

    // The first case is inside all three strict similarity thresholds.
    // Each other case violates one condition and therefore must not suppress.
    const auto allConditions = runCase(0.5, 0.1, 1.0);
    const auto positionBoundary = runCase(4.1, 0.1, 1.0);
    const auto speedBoundary = runCase(0.5, 0.51, 1.0);
    const auto headingBoundary = runCase(0.5, 0.1, 4.1);
    const bool allConditionsSuppress = allConditions.first &&
        allConditions.second == VamGenerationOutcome::SuppressedByRedundancy;
    const bool positionConditionRequired = positionBoundary.first &&
        positionBoundary.second != VamGenerationOutcome::SuppressedByRedundancy;
    const bool speedConditionRequired = speedBoundary.first &&
        speedBoundary.second != VamGenerationOutcome::SuppressedByRedundancy;
    const bool headingConditionRequired = headingBoundary.first &&
        headingBoundary.second != VamGenerationOutcome::SuppressedByRedundancy;
    Check(
        allConditionsSuppress && positionConditionRequired &&
            speedConditionRequired && headingConditionRequired,
        "PEER_REDUNDANCY_REQUIRES_ALL_THREE_STRICT_SIMILARITY_TESTS");
    return allConditionsSuppress && positionConditionRequired &&
           speedConditionRequired && headingConditionRequired;
}

bool TestRedundancyAlternativeSignalsAndSkipLimit()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;
    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);
    SetEastOffsetMeters(input, 5.0);

    VamGenerationContext context{};
    context.redundancy.inProtectedOrNonDrivableArea = true;

    for (std::int64_t at = 100; at <= 400; at += 100)
    {
        error.clear();
        ok = Evaluate(manager, at, input, context, result, error) && ok;
        ok = ok &&
             result.outcome == VamGenerationOutcome::SuppressedByRedundancy;
    }

    // The configured default is four skips. The fifth candidate must proceed.
    error.clear();
    ok = Evaluate(manager, 500, input, context, result, error) && ok;
    const bool skipLimitHonoured =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.shouldGenerate;
    Check(
        ok && skipLimitHonoured,
        "REDUNDANCY_SKIPS_ARE_BOUNDED_BY_CONFIGURED_NUM_SKIP_COUNT");
    return ok && skipLimitHonoured;
}

bool TestOtherRedundancyAlternatives()
{
    const auto isSuppressed = [](const VamRedundancyContext& redundancy) {
        VamGenerationManager manager;
        auto input = PedestrianInput();
        VamGenerationResult result{};
        std::string error;
        bool ok = Evaluate(manager, 0, input, {}, result, error) &&
                  Confirm(manager, result, 0, error);
        SetEastOffsetMeters(input, 5.0);
        VamGenerationContext context{};
        context.redundancy = redundancy;
        error.clear();
        ok = Evaluate(manager, 100, input, context, result, error) && ok;
        return ok && result.outcome ==
                         VamGenerationOutcome::SuppressedByRedundancy;
    };

    VamRedundancyContext protectedArea{};
    protectedArea.inProtectedOrNonDrivableArea = true;
    VamRedundancyContext lowRiskArea{};
    lowRiskArea.inPedestrianOnlyLowRiskArea = true;
    VamRedundancyContext reportedElsewhere{};
    reportedElsewhere.egoReportedByAnotherItsSWithinTGenVam = true;

    const bool allAlternativesSuppress =
        isSuppressed(protectedArea) && isSuppressed(lowRiskArea) &&
        isSuppressed(reportedElsewhere);
    Check(
        allAlternativesSuppress,
        "REDUNDANCY_ALTERNATIVE_SUPPRESSION_SIGNALS_ARE_INDEPENDENT");
    return allAlternativesSuppress;
}

bool TestRedundancyPeerTimeHorizon()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;
    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    SetEastOffsetMeters(input, 5.0);
    VamGenerationContext context{};
    VamPeerObservation peer{};
    peer.referencePosition = input.referencePosition;
    peer.highFrequencyState = input.highFrequencyState;
    context.redundancy.similarPeer = peer;

    error.clear();
    ok = Evaluate(manager, 20001, input, context, result, error) && ok;
    const bool oldTransmissionCannotSuppress =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.shouldGenerate;
    Check(
        ok && oldTransmissionCannotSuppress,
        "PEER_REDUNDANCY_TIME_HORIZON_IS_BOUNDED_BY_SKIP_COUNT_TIMES_MAX_INTERVAL");
    return ok && oldTransmissionCannotSuppress;
}

bool TestRedundancySkipCountConfigRange()
{
    auto rejected = [](std::uint8_t skipCount) {
        VamGenerationConfig config{};
        config.numSkipVamsForRedundancyMitigation = skipCount;
        VamGenerationManager manager(config);
        VamGenerationResult result{};
        std::string error;
        return !Evaluate(
            manager, 0, PedestrianInput(), {}, result, error) && !error.empty();
    };

    const bool belowRangeRejected = rejected(1);
    const bool aboveRangeRejected = rejected(11);
    Check(
        belowRangeRejected && aboveRangeRejected,
        "REDUNDANCY_SKIP_COUNT_CONFIG_MUST_BE_WITHIN_2_TO_10");
    return belowRangeRejected && aboveRangeRejected;
}

bool TestClusterOperationRequiresIndividualVamGeneration()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    // The individual VAM announcing a cluster operation is a generation
    // trigger even when position, speed, orientation and trajectory are
    // unchanged. The operation container itself is already part of VamInput.
    input.clusterOperation = VamClusterOperation{};
    error.clear();
    ok = Evaluate(manager, 100, input, {}, result, error) && ok;

    const bool operationTriggersVam =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.shouldGenerate;

    Check(
        ok && operationTriggersVam,
        "CLUSTER_OPERATION_CHANGE_TRIGGERS_INDIVIDUAL_VAM");
    return ok && operationTriggersVam;
}


VamSafeProximityObservation MakeSafeProximityObservation(
    std::int64_t firstObservedAtMs,
    double lateralDistanceMeters,
    double longitudinalDistanceMeters,
    double verticalDistanceMeters)
{
    VamSafeProximityObservation observation{};
    observation.firstObservedAtMs = firstObservedAtMs;
    observation.lateralDistanceMeters = lateralDistanceMeters;
    observation.longitudinalDistanceMeters = longitudinalDistanceMeters;
    observation.verticalDistanceMeters = verticalDistanceMeters;

    // Fixture de limiares fornecidos pela camada de percepção.
    // Não representam constantes universais do Generation Manager.
    observation.minimumSafeLateralDistanceMeters = 10.0;
    observation.minimumSafeLongitudinalDistanceMeters = 10.0;
    observation.minimumSafeVerticalDistanceMeters = 5.0;

    return observation;
}

bool TestSafeProximityRequiresAllThreeStrictThresholds()
{
    const auto runCase = [](double lateral,
                            double longitudinal,
                            double vertical) {
        VamGenerationManager manager;
        auto input = PedestrianInput();
        VamGenerationResult result{};
        std::string error;

        bool ok = Evaluate(manager, 0, input, {}, result, error) &&
                  Confirm(manager, result, 0, error);

        VamGenerationContext context{};
        context.safeProximityObservations.push_back(
            MakeSafeProximityObservation(1, lateral, longitudinal, vertical));

        error.clear();
        ok = Evaluate(manager, 100, input, context, result, error) && ok;

        return std::make_pair(ok, result);
    };

    const auto allInside = runCase(9.9, 9.9, 4.9);
    const auto lateralBoundary = runCase(10.0, 9.9, 4.9);
    const auto longitudinalBoundary = runCase(9.9, 10.0, 4.9);
    const auto verticalBoundary = runCase(9.9, 9.9, 5.0);

    const bool allThreeRequired =
        allInside.first &&
        allInside.second.outcome ==
            VamGenerationOutcome::GenerationCandidate &&
        allInside.second.reason ==
            VamGenerationReason::NewParticipantWithinSafeDistance;

    const bool lateralMustBeStrict =
        lateralBoundary.first &&
        lateralBoundary.second.outcome == VamGenerationOutcome::NoTrigger;

    const bool longitudinalMustBeStrict =
        longitudinalBoundary.first &&
        longitudinalBoundary.second.outcome == VamGenerationOutcome::NoTrigger;

    const bool verticalMustBeStrict =
        verticalBoundary.first &&
        verticalBoundary.second.outcome == VamGenerationOutcome::NoTrigger;

    Check(
        allThreeRequired && lateralMustBeStrict &&
            longitudinalMustBeStrict && verticalMustBeStrict,
        "SAFE_PROXIMITY_TRIGGER_REQUIRES_ALL_THREE_STRICT_DISTANCE_CONDITIONS");

    return allThreeRequired && lateralMustBeStrict &&
           longitudinalMustBeStrict && verticalMustBeStrict;
}

bool TestSafeProximityRequiresNewParticipantAfterLastConfirmedVam()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    VamGenerationContext context{};
    context.safeProximityObservations.push_back(
        MakeSafeProximityObservation(0, 9.0, 9.0, 4.0));

    error.clear();
    ok = Evaluate(manager, 100, input, context, result, error) && ok;
    const bool oldParticipantIgnored =
        result.outcome == VamGenerationOutcome::NoTrigger;

    context.safeProximityObservations.front().firstObservedAtMs = 1;

    error.clear();
    ok = Evaluate(manager, 200, input, context, result, error) && ok;

    const bool newParticipantTriggers =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.reason ==
            VamGenerationReason::NewParticipantWithinSafeDistance;

    const bool confirmed = Confirm(manager, result, 200, error) && ok;

    error.clear();
    ok = Evaluate(manager, 300, input, context, result, error) && ok;

    const bool confirmedParticipantIsNoLongerNew =
        result.outcome == VamGenerationOutcome::NoTrigger &&
        !result.shouldGenerate;

    Check(
        ok && oldParticipantIgnored && newParticipantTriggers &&
            confirmed && confirmedParticipantIsNoLongerNew,
        "SAFE_PROXIMITY_TRIGGER_ONLY_CONSUMES_NEW_PARTICIPANTS");

    return ok && oldParticipantIgnored && newParticipantTriggers &&
           confirmed && confirmedParticipantIsNoLongerNew;
}

bool TestSafeProximityHonoursMinimumIntervalAndTransactionLifecycle()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    bool ok = Evaluate(manager, 0, input, {}, result, error) &&
              Confirm(manager, result, 0, error);

    VamGenerationContext context{};
    context.safeProximityObservations.push_back(
        MakeSafeProximityObservation(50, 9.0, 9.0, 4.0));

    error.clear();
    ok = Evaluate(manager, 99, input, context, result, error) && ok;
    const bool minimumIntervalRespected =
        result.outcome == VamGenerationOutcome::NotDue &&
        !result.shouldGenerate;

    error.clear();
    ok = Evaluate(manager, 100, input, context, result, error) && ok;
    const bool candidateReady =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.reason ==
            VamGenerationReason::NewParticipantWithinSafeDistance;

    const bool aborted =
        result.transactionId.has_value() &&
        manager.AbortGeneration(*result.transactionId, error);

    error.clear();
    ok = Evaluate(manager, 100, input, context, result, error) && ok;
    const bool retryAfterAbort =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.reason ==
            VamGenerationReason::NewParticipantWithinSafeDistance;

    const bool confirmed = Confirm(manager, result, 100, error) && ok;

    error.clear();
    ok = Evaluate(manager, 200, input, context, result, error) && ok;
    const bool noRepeatedTrigger =
        result.outcome == VamGenerationOutcome::NoTrigger;

    Check(
        ok && minimumIntervalRespected && candidateReady && aborted &&
            retryAfterAbort && confirmed && noRepeatedTrigger,
        "SAFE_PROXIMITY_TRIGGER_RESPECTS_MIN_INTERVAL_AND_TRANSACTION");

    return ok && minimumIntervalRespected && candidateReady && aborted &&
           retryAfterAbort && confirmed && noRepeatedTrigger;
}


bool TestSafeProximityUsesConfirmedTransmissionTimestamp()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    bool ok = Evaluate(manager, 0, input, {}, result, error);
    const auto initialToken = result.transactionId;

    // A decisão é avaliada em 0 ms, mas a transmissão só é confirmada
    // em 25 ms, depois da montagem/entrega do VAM.
    error.clear();
    const bool initialConfirmed =
        initialToken.has_value() &&
        manager.ConfirmGeneration(*initialToken, 25, error);

    VamGenerationContext context{};
    context.safeProximityObservations.push_back(
        MakeSafeProximityObservation(10, 9.0, 9.0, 4.0));

    // A observação ocorreu depois de Evaluate(0), mas antes da transmissão
    // confirmada em 25 ms: não deve gerar um novo VAM.
    error.clear();
    ok = Evaluate(manager, 125, input, context, result, error) && ok;
    const bool observationDuringAssemblyIgnored =
        result.outcome == VamGenerationOutcome::NoTrigger;

    // A igualdade com o instante de transmissão também não satisfaz
    // a condição estrita "observado depois da última transmissão".
    context.safeProximityObservations.front().firstObservedAtMs = 25;
    error.clear();
    ok = Evaluate(manager, 225, input, context, result, error) && ok;
    const bool exactTransmissionBoundaryIgnored =
        result.outcome == VamGenerationOutcome::NoTrigger;

    // Só uma observação posterior à confirmação da transmissão é elegível.
    context.safeProximityObservations.front().firstObservedAtMs = 26;
    error.clear();
    ok = Evaluate(manager, 325, input, context, result, error) && ok;
    const bool postTransmissionObservationTriggers =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.reason ==
            VamGenerationReason::NewParticipantWithinSafeDistance;

    // A geração seguinte deve respeitar T_GenVamMin a partir da confirmação
    // efetiva em 345 ms, não da avaliação em 325 ms.
    error.clear();
    const bool candidateConfirmed =
        Confirm(manager, result, 345, error) && ok;

    context.safeProximityObservations.push_back(
        MakeSafeProximityObservation(346, 9.0, 9.0, 4.0));

    error.clear();
    ok = Evaluate(manager, 444, input, context, result, error) && ok;
    const bool minIntervalUsesTransmissionTime =
        result.outcome == VamGenerationOutcome::NotDue;

    error.clear();
    ok = Evaluate(manager, 445, input, context, result, error) && ok;
    const bool intervalAllowsGeneration =
        result.outcome == VamGenerationOutcome::GenerationCandidate &&
        result.reason ==
            VamGenerationReason::NewParticipantWithinSafeDistance;

    const bool candidateAborted =
        result.transactionId.has_value() &&
        manager.AbortGeneration(*result.transactionId, error);

    // Uma confirmação anterior à avaliação da transação deve ser rejeitada,
    // mas a mesma transação continua confirmável num instante válido.
    VamGenerationManager invalidConfirmationManager;
    error.clear();
    const bool secondEvaluationAccepted =
        Evaluate(invalidConfirmationManager, 50, input, {},
                 result, error);
    const auto secondToken = result.transactionId;

    error.clear();
    const bool earlierConfirmationRejected =
        secondToken.has_value() &&
        !invalidConfirmationManager.ConfirmGeneration(
            *secondToken, 49, error);

    error.clear();
    const bool validConfirmationAccepted =
        secondToken.has_value() &&
        invalidConfirmationManager.ConfirmGeneration(
            *secondToken, 50, error);

    const bool passed =
        ok && initialConfirmed &&
        observationDuringAssemblyIgnored &&
        exactTransmissionBoundaryIgnored &&
        postTransmissionObservationTriggers &&
        candidateConfirmed && minIntervalUsesTransmissionTime &&
        intervalAllowsGeneration && candidateAborted &&
        secondEvaluationAccepted && earlierConfirmationRejected &&
        validConfirmationAccepted;

    Check(
        passed,
        "SAFE_PROXIMITY_AND_MIN_INTERVAL_USE_CONFIRMED_TRANSMISSION_TIME");

    return passed;
}


bool TestInvalidSafeProximityObservationsPreserveManagerState()
{
    const auto validObservation =
        MakeSafeProximityObservation(1, 1.0, 1.0, 1.0);

    std::vector<VamSafeProximityObservation> invalidObservations;

    auto negativeTimestamp = validObservation;
    negativeTimestamp.firstObservedAtMs = -1;
    invalidObservations.push_back(negativeTimestamp);

    auto futureTimestamp = validObservation;
    futureTimestamp.firstObservedAtMs = 101;
    invalidObservations.push_back(futureTimestamp);

    const std::array<
        double VamSafeProximityObservation::*, 6> numericFields{{
        &VamSafeProximityObservation::lateralDistanceMeters,
        &VamSafeProximityObservation::longitudinalDistanceMeters,
        &VamSafeProximityObservation::verticalDistanceMeters,
        &VamSafeProximityObservation::minimumSafeLateralDistanceMeters,
        &VamSafeProximityObservation::minimumSafeLongitudinalDistanceMeters,
        &VamSafeProximityObservation::minimumSafeVerticalDistanceMeters
    }};

    const std::array<double, 3> invalidNumericValues{{
        -1.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()
    }};

    for (const auto field : numericFields)
    {
        for (const double invalidValue : invalidNumericValues)
        {
            auto observation = validObservation;
            observation.*field = invalidValue;
            invalidObservations.push_back(observation);
        }
    }

    bool allRejectedWithoutMutation = true;
    std::size_t testedCases = 0;

    for (const auto& invalidObservation : invalidObservations)
    {
        VamGenerationManager manager;
        auto input = PedestrianInput();
        VamGenerationResult initial{};
        std::string error;

        const bool initialEvaluated =
            Evaluate(manager, 0, input, {}, initial, error);
        const bool initialConfirmed =
            initialEvaluated && Confirm(manager, initial, 0, error);

        VamGenerationContext invalidContext{};
        invalidContext.safeProximityObservations.push_back(
            invalidObservation);

        VamGenerationResult output{};
        output.outcome = VamGenerationOutcome::BlockedByPolicy;
        output.shouldGenerate = true;
        output.reason =
            VamGenerationReason::TrajectoryInterceptionProbabilityChanged;
        output.includeLowFrequencyContainer = true;
        output.effectiveTGenVamMs = 777;
        output.transactionId = 999;

        error.clear();
        const bool invalidInputRejected =
            !Evaluate(manager, 100, input, invalidContext, output, error) &&
            !error.empty();

        const bool outputPreserved =
            output.outcome == VamGenerationOutcome::BlockedByPolicy &&
            output.shouldGenerate &&
            output.reason ==
                VamGenerationReason::TrajectoryInterceptionProbabilityChanged &&
            output.includeLowFrequencyContainer &&
            output.effectiveTGenVamMs == 777 &&
            output.transactionId == 999;

        // A rejeição em 100 ms não pode avançar o relógio interno:
        // uma avaliação válida em 99 ms ainda deve ser aceite.
        VamGenerationResult recovery{};
        error.clear();
        const bool recoveryAccepted =
            Evaluate(manager, 99, input, {}, recovery, error);

        const bool managerStatePreserved =
            recoveryAccepted &&
            error.empty() &&
            recovery.outcome == VamGenerationOutcome::NotDue &&
            !recovery.shouldGenerate;

        allRejectedWithoutMutation =
            initialConfirmed &&
            invalidInputRejected &&
            outputPreserved &&
            managerStatePreserved &&
            allRejectedWithoutMutation;

        ++testedCases;
    }

    const bool passed =
        allRejectedWithoutMutation && testedCases == 20;

    Check(
        passed,
        "INVALID_SAFE_PROXIMITY_OBSERVATIONS_REJECT_WITHOUT_MUTATING_MANAGER");

    return passed;
}

bool TestTransactionsAreAtomicAndSingleUse()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult result{};
    std::string error;

    bool ok = Evaluate(manager, 0, input, {}, result, error);
    const auto token = result.transactionId;
    const auto preservedOutcome = result.outcome;

    error.clear();
    const bool concurrentEvaluationRejected =
        !Evaluate(manager, 100, input, {}, result, error);
    const bool outputPreserved = result.outcome == preservedOutcome &&
                                 result.transactionId == token;

    error.clear();
    const bool aborted = token.has_value() &&
                         manager.AbortGeneration(*token, error);

    error.clear();
    const bool staleConfirmRejected = token.has_value() &&
        !manager.ConfirmGeneration(*token, 100, error);

    error.clear();
    const bool retryAccepted = Evaluate(manager, 100, input, {}, result, error);
    const auto retryToken = result.transactionId;
    error.clear();
    const bool retryConfirmed = retryToken.has_value() &&
        manager.ConfirmGeneration(*retryToken, 100, error);

    error.clear();
    const bool repeatedConfirmRejected = retryToken.has_value() &&
        !manager.ConfirmGeneration(*retryToken, 100, error);

    Check(
        ok && concurrentEvaluationRejected && outputPreserved && aborted &&
            staleConfirmRejected && retryAccepted && retryConfirmed &&
            repeatedConfirmRejected,
        "GENERATION_CONFIRM_ABORT_AND_TOKEN_LIFECYCLE_ARE_ATOMIC");
    return ok && concurrentEvaluationRejected && outputPreserved && aborted &&
           staleConfirmRejected && retryAccepted && retryConfirmed &&
           repeatedConfirmRejected;
}

bool TestInvalidTimeAndConfigPreserveOutput()
{
    VamGenerationManager manager;
    auto input = PedestrianInput();
    VamGenerationResult output{};
    output.effectiveTGenVamMs = 777;
    output.shouldGenerate = true;
    std::string error;

    const bool negativeTimeRejected =
        !Evaluate(manager, -1, input, {}, output, error);
    const bool outputPreserved =
        output.effectiveTGenVamMs == 777 && output.shouldGenerate;

    VamGenerationConfig invalidConfig{};
    invalidConfig.tCheckVamGenMs = 101;
    VamGenerationManager invalidManager(invalidConfig);
    error.clear();
    const bool invalidCheckPeriodRejected =
        !Evaluate(invalidManager, 0, input, {}, output, error);

    Check(
        negativeTimeRejected && outputPreserved &&
            invalidCheckPeriodRejected && !error.empty(),
        "INVALID_TIME_AND_TIMER_CONFIG_REJECT_WITHOUT_MUTATING_OUTPUT");
    return negativeTimeRejected && outputPreserved &&
           invalidCheckPeriodRejected;
}

} // namespace

int main()
{
    TestInitialGenerationAndMinimumInterval();
    TestActivationTransitionTriggersImmediately();
    TestPositionThresholdIsStrict();
    TestSpeedThresholdIsStrict();
    TestOrientationWrapAndStrictThreshold();
    TestMaximumGenerationIntervalIsStrict();
    TestTrajectoryProbabilityUsesPercentagePoints();
    TestLowFrequencyContainerPeriodicity();
    TestDccIntervalClamping();
    TestRoleAndVbsPolicyGate();
    TestRedundancyPeerRequiresAllSimilarityConditions();
    TestRedundancyAlternativeSignalsAndSkipLimit();
    TestOtherRedundancyAlternatives();
    TestRedundancyPeerTimeHorizon();
    TestRedundancySkipCountConfigRange();
    TestClusterOperationRequiresIndividualVamGeneration();
    TestSafeProximityRequiresAllThreeStrictThresholds();
    TestSafeProximityRequiresNewParticipantAfterLastConfirmedVam();
    TestSafeProximityHonoursMinimumIntervalAndTransactionLifecycle();
    TestSafeProximityUsesConfirmedTransmissionTimestamp();
    TestInvalidSafeProximityObservationsPreserveManagerState();
    TestTransactionsAreAtomicAndSingleUse();
    TestInvalidTimeAndConfigPreserveOutput();

    if (failures != 0)
    {
        std::cout << "VAM_GENERATION_MANAGER=FAIL failures=" << failures << '\n';
        return 1;
    }

    std::cout << "VAM_GENERATION_MANAGER=PASS\n";
    return 0;
}
