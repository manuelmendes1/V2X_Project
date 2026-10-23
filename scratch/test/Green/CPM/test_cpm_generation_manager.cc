#include "vanetza_integration/cpm/perception/cpm_generation_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace
{

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << label << '\n';
    return condition;
}

CpmGenerationCandidate Candidate(
    std::uint16_t id,
    CpmGenerationObjectType type = CpmGenerationObjectType::TypeB,
    double x = 0.0,
    std::optional<std::uint8_t> quality = 4)
{
    CpmGenerationCandidate candidate{};
    candidate.objectId = id;
    candidate.type = type;
    candidate.xEastMeters = x;
    candidate.yNorthMeters = 0.0;
    candidate.groundSpeedMetersPerSecond = 10.0;
    candidate.groundVelocityOrientationDeg = 10.0;
    candidate.objectPerceptionQuality = quality;
    candidate.estimatedPayloadBytes = 50;
    return candidate;
}

bool ContainsId(
    const CpmGenerationResult& result,
    std::uint16_t id)
{
    return std::find(
        result.includedObjectIds.begin(),
        result.includedObjectIds.end(),
        id) != result.includedObjectIds.end();
}

const CpmGenerationDecision* DecisionFor(
    const CpmGenerationResult& result,
    std::uint16_t id)
{
    const auto found = std::find_if(
        result.decisions.begin(),
        result.decisions.end(),
        [id](const CpmGenerationDecision& decision) {
            return decision.objectId == id;
        });

    return found == result.decisions.end() ? nullptr : &*found;
}

bool EvaluateAndConfirm(
    CpmGenerationManager& manager,
    std::int64_t nowMs,
    const std::vector<CpmGenerationCandidate>& candidates,
    CpmGenerationResult& result,
    std::string& error)
{
    if (!manager.Evaluate(nowMs, candidates, result, error))
    {
        return false;
    }

    if (!result.transactionId.has_value())
    {
        return true;
    }

    return manager.ConfirmMessageCreated(*result.transactionId, error);
}

bool TestFirstDetectionAndGenerationInterval()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;
    const auto object = Candidate(1);

    bool accepted =
        EvaluateAndConfirm(manager, 0, {object}, result, error);

    bool correct =
        accepted && error.empty() &&
        result.generationEventDue &&
        result.shouldCreateMessage &&
        result.knownObjectCount == 1 &&
        result.includedObjectIds == std::vector<std::uint16_t>{1};

    bool passed = Check(correct, "FIRST_DETECTION_IS_INCLUDED");

    error.clear();
    accepted = EvaluateAndConfirm(manager, 50, {object}, result, error);
    correct =
        accepted && error.empty() &&
        !result.generationEventDue &&
        result.includedObjectIds.empty();

    passed = Check(correct, "GENERATION_INTERVAL_PREVENTS_EARLY_EVENT")
             && passed;

    error.clear();
    accepted = EvaluateAndConfirm(manager, 100, {object}, result, error);
    correct =
        accepted && error.empty() &&
        result.generationEventDue &&
        !result.shouldCreateMessage &&
        result.includedObjectIds.empty();

    passed = Check(correct, "UNCHANGED_OBJECT_IS_NOT_REINCLUDED_EARLY")
             && passed;

    error.clear();
    accepted = EvaluateAndConfirm(manager, 1000, {object}, result, error);
    correct =
        accepted && error.empty() &&
        result.generationEventDue &&
        ContainsId(result, 1);

    const auto* decision = DecisionFor(result, 1);
    correct = correct && decision != nullptr &&
        decision->reason ==
            CpmGenerationDecisionReason::MaximumInclusionInterval;

    passed = Check(correct, "MAXIMUM_INCLUSION_INTERVAL_REFRESH")
             && passed;

    return passed;
}

bool TestPositionChangeThreshold()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;

    bool accepted = EvaluateAndConfirm(manager,
        0, {Candidate(2)}, result, error);

    bool correct = accepted && ContainsId(result, 2);

    error.clear();
    accepted = EvaluateAndConfirm(manager,
        100, {Candidate(2, CpmGenerationObjectType::TypeB, 4.0)},
        result, error);

    correct = correct && accepted && result.includedObjectIds.empty();

    error.clear();
    accepted = EvaluateAndConfirm(manager,
        200, {Candidate(2, CpmGenerationObjectType::TypeB, 4.001)},
        result, error);

    correct = correct && accepted && ContainsId(result, 2);

    const auto* decision = DecisionFor(result, 2);
    correct = correct && decision != nullptr &&
        decision->reason == CpmGenerationDecisionReason::PositionChanged;

    return Check(correct, "POSITION_CHANGE_THRESHOLD_IS_STRICT");
}

bool TestGroundSpeedChangeThreshold()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;
    auto object = Candidate(3);

    bool accepted = EvaluateAndConfirm(manager, 0, {object}, result, error);
    bool correct = accepted && ContainsId(result, 3);

    object.groundSpeedMetersPerSecond = 10.5;
    error.clear();
    accepted = EvaluateAndConfirm(manager, 100, {object}, result, error);
    correct = correct && accepted && result.includedObjectIds.empty();

    object.groundSpeedMetersPerSecond = 10.5001;
    error.clear();
    accepted = EvaluateAndConfirm(manager, 200, {object}, result, error);
    correct = correct && accepted && ContainsId(result, 3);

    const auto* decision = DecisionFor(result, 3);
    correct = correct && decision != nullptr &&
        decision->reason == CpmGenerationDecisionReason::GroundSpeedChanged;

    return Check(correct, "GROUND_SPEED_CHANGE_THRESHOLD_IS_STRICT");
}

bool TestGroundVelocityOrientationChange()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;
    auto object = Candidate(4);

    object.groundVelocityOrientationDeg = 359.0;
    bool accepted = EvaluateAndConfirm(manager, 0, {object}, result, error);
    bool correct = accepted && ContainsId(result, 4);

    object.groundVelocityOrientationDeg = 3.0;
    error.clear();
    accepted = EvaluateAndConfirm(manager, 100, {object}, result, error);

    correct = correct && accepted && ContainsId(result, 4);
    const auto* decision = DecisionFor(result, 4);

    correct = correct && decision != nullptr &&
        decision->reason ==
            CpmGenerationDecisionReason::GroundVelocityOrientationChanged;

    return Check(correct, "ORIENTATION_CHANGE_HANDLES_WRAP_AT_360");
}

bool TestTypeABatchRefresh()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;

    const auto first =
        Candidate(10, CpmGenerationObjectType::TypeA);
    bool accepted = EvaluateAndConfirm(manager, 0, {first}, result, error);
    bool correct = accepted && ContainsId(result, 10);

    const auto second =
        Candidate(20, CpmGenerationObjectType::TypeA);

    error.clear();
    accepted = EvaluateAndConfirm(manager, 100, {first, second}, result, error);
    correct = correct && accepted && ContainsId(result, 20) &&
        !ContainsId(result, 10);

    error.clear();
    accepted = EvaluateAndConfirm(manager, 500, {first, second}, result, error);

    correct = correct && accepted &&
        ContainsId(result, 10) &&
        ContainsId(result, 20);

    return Check(correct, "TYPE_A_HALF_MAX_INTERVAL_REFRESHES_GROUP");
}

bool TestPerceptionQualityThreshold()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;

    bool accepted = EvaluateAndConfirm(manager,
        0,
        {Candidate(30, CpmGenerationObjectType::TypeB, 0.0, 3)},
        result,
        error);

    bool correct =
        accepted && result.generationEventDue &&
        !result.shouldCreateMessage &&
        !ContainsId(result, 30);

    const auto* decision = DecisionFor(result, 30);
    correct = correct && decision != nullptr &&
        decision->reason ==
            CpmGenerationDecisionReason::BelowPerceptionQualityThreshold;

    error.clear();
    accepted = EvaluateAndConfirm(manager,
        100,
        {Candidate(30, CpmGenerationObjectType::TypeB, 0.0, 4)},
        result,
        error);

    correct = correct && accepted && ContainsId(result, 30);

    const auto* eligibleDecision = DecisionFor(result, 30);
    correct = correct && eligibleDecision != nullptr &&
        eligibleDecision->reason ==
            CpmGenerationDecisionReason::FirstEligibleInclusion;

    return Check(correct, "QUALITY_MUST_EXCEED_CONFIGURED_THRESHOLD");
}


bool TestPerceptionQualityDomain()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;

    const bool maximumAccepted = EvaluateAndConfirm(manager,
        0,
        {Candidate(31, CpmGenerationObjectType::TypeB, 0.0, 15)},
        result,
        error);

    bool passed = Check(
        maximumAccepted && error.empty() && ContainsId(result, 31),
        "MAX_OBJECT_PERCEPTION_QUALITY_15_IS_VALID");

    const auto previousIds = result.includedObjectIds;
    const auto previousKnownCount = result.knownObjectCount;

    error.clear();
    const bool outOfRangeRejected = !EvaluateAndConfirm(manager,
        100,
        {Candidate(32, CpmGenerationObjectType::TypeB, 0.0, 16)},
        result,
        error);

    const bool outputPreserved =
        result.includedObjectIds == previousIds &&
        result.knownObjectCount == previousKnownCount;

    passed = Check(
        outOfRangeRejected && !error.empty() && outputPreserved,
        "QUALITY_16_REJECTED_WITH_ATOMIC_OUTPUT") && passed;

    return passed;
}


bool TestPerceptionQualityThresholdDomain()
{
    CpmGenerationConfig validConfig{};
    validConfig.objectPerceptionQualityThreshold = 15;

    CpmGenerationManager validManager(validConfig);
    CpmGenerationResult result{};
    std::string error;

    const bool validThresholdAccepted = validManager.Evaluate(
        0,
        {Candidate(33, CpmGenerationObjectType::TypeB, 0.0, 15)},
        result,
        error);

    const bool strictThresholdRespected =
        validThresholdAccepted &&
        error.empty() &&
        result.generationEventDue &&
        result.knownObjectCount == 1 &&
        !result.shouldCreateMessage &&
        !ContainsId(result, 33);

    CpmGenerationConfig invalidConfig{};
    invalidConfig.objectPerceptionQualityThreshold = 16;

    CpmGenerationManager invalidManager(invalidConfig);
    CpmGenerationResult preserved{};
    preserved.knownObjectCount = 456;

    error.clear();
    const bool invalidThresholdRejected =
        !invalidManager.Evaluate(0, {}, preserved, error);

    const bool outputPreserved =
        preserved.knownObjectCount == 456 &&
        !preserved.generationEventDue &&
        !preserved.shouldCreateMessage;

    return Check(
        strictThresholdRespected &&
        invalidThresholdRejected &&
        !error.empty() &&
        outputPreserved,
        "QUALITY_THRESHOLD_DOMAIN_AND_STRICT_COMPARISON");
}


bool TestGenerationOutcomeDiagnostics()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;

    bool accepted = EvaluateAndConfirm(manager, 0, {Candidate(41)}, result, error);
    bool correct = accepted && error.empty() &&
        result.generationEventDue && result.shouldCreateMessage &&
        result.outcome ==
            CpmGenerationOutcome::MessageCandidateSetProduced;
    bool passed = Check(correct, "OUTCOME_REPORTS_SELECTED_MESSAGE_CANDIDATES");

    error.clear();
    accepted = EvaluateAndConfirm(manager, 50, {Candidate(41)}, result, error);
    correct = accepted && error.empty() &&
        !result.generationEventDue &&
        result.outcome == CpmGenerationOutcome::IntervalNotElapsed;
    passed = Check(correct, "OUTCOME_REPORTS_INTERVAL_NOT_ELAPSED") && passed;

    error.clear();
    accepted = EvaluateAndConfirm(manager, 100, {}, result, error);
    correct = accepted && error.empty() &&
        result.generationEventDue && !result.shouldCreateMessage &&
        result.outcome ==
            CpmGenerationOutcome::EventDueNoEligibleObjects;
    passed = Check(correct, "OUTCOME_REPORTS_EVENT_WITHOUT_MESSAGE") && passed;

    return passed;
}

bool TestKnownCountAndEmptySelection()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;

    std::vector<CpmGenerationCandidate> objects{
        Candidate(1),
        Candidate(2, CpmGenerationObjectType::TypeB, 0.0, 3),
        Candidate(3, CpmGenerationObjectType::TypeB, 0.0, 3)
    };

    bool accepted = EvaluateAndConfirm(manager, 0, objects, result, error);
    bool correct =
        accepted && result.knownObjectCount == 3 &&
        result.includedObjectIds.size() == 1 &&
        ContainsId(result, 1);

    error.clear();
    accepted = EvaluateAndConfirm(manager, 100, {}, result, error);

    correct = correct && accepted &&
        result.generationEventDue &&
        result.knownObjectCount == 0 &&
        result.includedObjectIds.empty() &&
        !result.shouldCreateMessage;

    return Check(correct, "KNOWN_COUNT_DIFFERS_FROM_INCLUDED_COUNT");
}

bool TestCardinalityLimitAndStableOrdering()
{
    CpmGenerationConfig config{};
    config.maxObjectsPerMessage = 2;
    config.maxEstimatedPayloadBytes = 1000;

    CpmGenerationManager firstManager(config);
    CpmGenerationManager secondManager(config);
    CpmGenerationResult first{};
    CpmGenerationResult second{};
    std::string error;

    const std::vector<CpmGenerationCandidate> forward{
        Candidate(3), Candidate(1), Candidate(2)};
    const std::vector<CpmGenerationCandidate> reverse{
        Candidate(2), Candidate(3), Candidate(1)};

    bool accepted = firstManager.Evaluate(0, forward, first, error);
    error.clear();
    accepted = accepted &&
        secondManager.Evaluate(0, reverse, second, error);

    const bool correct =
        accepted &&
        first.includedObjectIds == std::vector<std::uint16_t>{1, 2} &&
        second.includedObjectIds == first.includedObjectIds &&
        first.knownObjectCount == 3 &&
        first.includedObjectIds.size() == 2 &&
        first.transactionId.has_value() &&
        second.transactionId.has_value();

    const auto* omitted = DecisionFor(first, 3);
    const bool rejectionVisible = omitted != nullptr &&
        !omitted->selected &&
        omitted->reason ==
            CpmGenerationDecisionReason::MaximumObjectCount;

    bool transactionsAborted = true;
    if (first.transactionId.has_value())
    {
        error.clear();
        transactionsAborted =
            firstManager.AbortMessageCreation(*first.transactionId, error) &&
            transactionsAborted && error.empty();
    }
    else
    {
        transactionsAborted = false;
    }

    if (second.transactionId.has_value())
    {
        error.clear();
        transactionsAborted =
            secondManager.AbortMessageCreation(*second.transactionId, error) &&
            transactionsAborted && error.empty();
    }
    else
    {
        transactionsAborted = false;
    }

    return Check(
        correct && rejectionVisible && transactionsAborted,
        "CARDINALITY_LIMIT_AND_ORDER_ARE_DETERMINISTIC");
}

bool TestEstimatedPayloadBudget()
{
    CpmGenerationConfig config{};
    config.maxObjectsPerMessage = 8;
    config.estimatedMessageOverheadBytes = 20;
    config.maxEstimatedPayloadBytes = 120;

    CpmGenerationManager manager(config);
    CpmGenerationResult result{};
    std::string error;

    bool accepted = EvaluateAndConfirm(manager,
        0,
        {Candidate(1), Candidate(2), Candidate(3)},
        result,
        error);

    const auto* omitted = DecisionFor(result, 3);
    const bool correct =
        accepted &&
        result.includedObjectIds == std::vector<std::uint16_t>{1, 2} &&
        result.estimatedPayloadBytes == 120 &&
        omitted != nullptr &&
        !omitted->selected &&
        omitted->reason ==
            CpmGenerationDecisionReason::PayloadBudgetExceeded;

    return Check(correct, "ESTIMATED_PAYLOAD_BUDGET_IS_ENFORCED");
}

bool TestObjectExpirationAndRediscovery()
{
    CpmGenerationConfig config{};
    config.objectRetentionMs = 2000;

    CpmGenerationManager manager(config);
    CpmGenerationResult result{};
    std::string error;

    bool accepted = EvaluateAndConfirm(manager,
        0, {Candidate(77)}, result, error);
    bool correct = accepted && ContainsId(result, 77);

    error.clear();
    accepted = EvaluateAndConfirm(manager, 100, {}, result, error);
    correct = correct && accepted && !result.shouldCreateMessage;

    error.clear();
    accepted = EvaluateAndConfirm(manager, 2100, {}, result, error);
    correct = correct && accepted && result.knownObjectCount == 0;

    error.clear();
    accepted = EvaluateAndConfirm(manager,
        2200, {Candidate(77)}, result, error);
    correct = correct && accepted && ContainsId(result, 77);

    const auto* decision = DecisionFor(result, 77);
    correct = correct && decision != nullptr &&
        decision->reason == CpmGenerationDecisionReason::FirstDetection;

    return Check(correct, "EXPIRED_OBJECT_IS_TREATED_AS_REDISCOVERED");
}

bool TestRejectedInputPreservesStateAndOutput()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    result.generationEventDue = true;
    result.knownObjectCount = 123;
    result.includedObjectIds = {99};

    std::string error;
    auto invalid = Candidate(88);
    invalid.xEastMeters = std::numeric_limits<double>::quiet_NaN();

    const bool rejected = !EvaluateAndConfirm(manager, 0, {invalid}, result, error);
    const bool outputPreserved =
        result.generationEventDue &&
        result.knownObjectCount == 123 &&
        result.includedObjectIds == std::vector<std::uint16_t>{99};

    error.clear();
    const bool recovered =
        EvaluateAndConfirm(manager, 0, {Candidate(88)}, result, error);

    const bool correct =
        rejected && outputPreserved && recovered &&
        error.empty() && ContainsId(result, 88);

    return Check(correct, "INVALID_INPUT_IS_REJECTED_ATOMICALLY");
}

bool TestConfigurationBounds()
{
    CpmGenerationConfig config{};
    config.generationIntervalMs = 99;

    CpmGenerationManager manager(config);
    CpmGenerationResult result{};
    result.knownObjectCount = 456;
    std::string error;

    const bool rejected = !EvaluateAndConfirm(manager, 0, {}, result, error);
    const bool preserved =
        result.knownObjectCount == 456 && !error.empty();

    return Check(
        rejected && preserved,
        "GENERATION_INTERVAL_OUTSIDE_PROFILE_RANGE_IS_REJECTED");
}


bool TestMessageCreationTransactionContract()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;
    const auto first = Candidate(101);

    const bool evaluated = manager.Evaluate(0, {first}, result, error);
    const std::uint64_t transactionId = result.transactionId.value_or(0);
    bool passed = Check(
        evaluated && error.empty() && result.shouldCreateMessage &&
            transactionId != 0,
        "MESSAGE_SELECTION_RETURNS_TRANSACTION_ID");

    error.clear();
    const bool wrongConfirmationRejected =
        transactionId != 0 &&
        !manager.ConfirmMessageCreated(transactionId + 1, error);
    passed = Check(
        wrongConfirmationRejected && !error.empty(),
        "UNKNOWN_MESSAGE_TRANSACTION_CANNOT_BE_CONFIRMED") && passed;

    CpmGenerationResult blockedOutput{};
    blockedOutput.generationEventDue = true;
    blockedOutput.knownObjectCount = 777;
    blockedOutput.includedObjectIds = {999};

    error.clear();
    const bool overlappingEvaluationRejected =
        !manager.Evaluate(100, {first}, blockedOutput, error);
    const bool blockedOutputPreserved =
        blockedOutput.generationEventDue &&
        blockedOutput.knownObjectCount == 777 &&
        blockedOutput.includedObjectIds == std::vector<std::uint16_t>{999};

    passed = Check(
        overlappingEvaluationRejected && blockedOutputPreserved &&
            !error.empty(),
        "PENDING_TRANSACTION_BLOCKS_EVALUATION_ATOMICALLY") && passed;

    error.clear();
    const bool confirmed = transactionId != 0 &&
        manager.ConfirmMessageCreated(transactionId, error);

    CpmGenerationResult afterConfirmation{};
    error.clear();
    const bool evaluatedAfterConfirmation = confirmed &&
        manager.Evaluate(100, {first}, afterConfirmation, error);

    const bool confirmationCommittedInclusion =
        evaluatedAfterConfirmation && error.empty() &&
        afterConfirmation.generationEventDue &&
        !afterConfirmation.shouldCreateMessage &&
        afterConfirmation.includedObjectIds.empty() &&
        !afterConfirmation.transactionId.has_value();

    passed = Check(
        confirmationCommittedInclusion,
        "CONFIRMED_MESSAGE_UPDATES_INCLUSION_STATE") && passed;

    CpmGenerationManager abortManager;
    CpmGenerationResult abortResult{};

    error.clear();
    const bool abortSelectionCreated = abortManager.Evaluate(
        0, {Candidate(102)}, abortResult, error);
    const std::uint64_t abortTransactionId =
        abortResult.transactionId.value_or(0);

    error.clear();
    const bool aborted = abortSelectionCreated && abortTransactionId != 0 &&
        abortManager.AbortMessageCreation(abortTransactionId, error);

    CpmGenerationResult afterAbort{};
    error.clear();
    const bool retriedAfterAbort = aborted &&
        abortManager.Evaluate(100, {Candidate(102)}, afterAbort, error);

    const bool abortPreservedUnconfirmedStatus =
        retriedAfterAbort && error.empty() && afterAbort.shouldCreateMessage &&
        ContainsId(afterAbort, 102) && afterAbort.transactionId.has_value();

    passed = Check(
        abortPreservedUnconfirmedStatus,
        "ABORTED_MESSAGE_DOES_NOT_MARK_OBJECT_AS_INCLUDED") && passed;

    if (afterAbort.transactionId.has_value())
    {
        error.clear();
        abortManager.AbortMessageCreation(*afterAbort.transactionId, error);
    }

    return passed;
}

bool TestAbortedTransactionPreservesMonotonicTime()
{
    CpmGenerationManager manager;
    CpmGenerationResult selected{};
    std::string error;

    const bool evaluated = manager.Evaluate(
        1000, {Candidate(103)}, selected, error);
    const auto transactionId = selected.transactionId;

    error.clear();
    const bool aborted = evaluated && transactionId.has_value() &&
        manager.AbortMessageCreation(*transactionId, error);

    CpmGenerationResult output{};
    output.generationEventDue = true;
    output.knownObjectCount = 456;
    output.includedObjectIds = {88};

    error.clear();
    const bool regressingTimeRejected =
        !manager.Evaluate(500, {Candidate(103)}, output, error);

    const bool outputPreserved =
        output.generationEventDue &&
        output.knownObjectCount == 456 &&
        output.includedObjectIds == std::vector<std::uint16_t>{88};

    return Check(
        evaluated && aborted && regressingTimeRejected &&
            !error.empty() && outputPreserved,
        "ABORT_PRESERVES_MONOTONIC_LOGICAL_TIME");
}

bool TestDuplicateIdsRejected()
{
    CpmGenerationManager manager;
    CpmGenerationResult result{};
    std::string error;

    const bool rejected = !manager.Evaluate(
        0,
        {Candidate(5), Candidate(5)},
        result,
        error);

    return Check(
        rejected && !error.empty() &&
        !result.generationEventDue,
        "DUPLICATE_OBJECT_IDS_ARE_REJECTED");
}

} // namespace

int main()
{
    bool passed = true;

    passed = TestFirstDetectionAndGenerationInterval() && passed;
    passed = TestGenerationOutcomeDiagnostics() && passed;
    passed = TestPositionChangeThreshold() && passed;
    passed = TestGroundSpeedChangeThreshold() && passed;
    passed = TestGroundVelocityOrientationChange() && passed;
    passed = TestTypeABatchRefresh() && passed;
    passed = TestPerceptionQualityThreshold() && passed;
    passed = TestPerceptionQualityDomain() && passed;
    passed = TestPerceptionQualityThresholdDomain() && passed;
    passed = TestKnownCountAndEmptySelection() && passed;
    passed = TestCardinalityLimitAndStableOrdering() && passed;
    passed = TestEstimatedPayloadBudget() && passed;
    passed = TestObjectExpirationAndRediscovery() && passed;
    passed = TestRejectedInputPreservesStateAndOutput() && passed;
    passed = TestConfigurationBounds() && passed;
    passed = TestMessageCreationTransactionContract() && passed;
    passed = TestAbortedTransactionPreservesMonotonicTime() && passed;
    passed = TestDuplicateIdsRejected() && passed;

    std::cout << "CPM_GENERATION_MANAGER="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
