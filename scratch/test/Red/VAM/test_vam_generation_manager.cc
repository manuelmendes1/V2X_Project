#include "vanetza_integration/vam/generation/vam_generation_manager.h"

#include <cmath>
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
    std::string& error)
{
    return result.transactionId.has_value() &&
           manager.ConfirmGeneration(*result.transactionId, error);
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

    const bool confirmed = Confirm(manager, result, error);
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
              Confirm(manager, result, error);

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
              Confirm(manager, result, error);

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
              Confirm(manager, result, error);

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
              Confirm(manager, result, error);

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
              Confirm(manager, result, error);

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
              Confirm(manager, result, error);

    SetEastOffsetMeters(input, 5.0);
    error.clear();
    ok = Evaluate(manager, 100, input, {}, result, error) && ok;
    const bool noEarlyLf = !result.includeLowFrequencyContainer;
    ok = Confirm(manager, result, error) && ok;

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
                  Confirm(manager, result, error);

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
              Confirm(manager, result, error);
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
                  Confirm(manager, result, error);
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
              Confirm(manager, result, error);

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
              Confirm(manager, result, error);

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
        !manager.ConfirmGeneration(*token, error);

    error.clear();
    const bool retryAccepted = Evaluate(manager, 100, input, {}, result, error);
    const auto retryToken = result.transactionId;
    error.clear();
    const bool retryConfirmed = retryToken.has_value() &&
        manager.ConfirmGeneration(*retryToken, error);

    error.clear();
    const bool repeatedConfirmRejected = retryToken.has_value() &&
        !manager.ConfirmGeneration(*retryToken, error);

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
