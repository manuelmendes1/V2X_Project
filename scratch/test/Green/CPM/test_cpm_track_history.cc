#include "vanetza_integration/cpm/perception/cpm_track_history.h"
#include "vanetza_integration/cpm/perception/cpm_synthetic_perception_source.h"

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

CpmSyntheticPerceptionScenario MakeScenario(
    std::int64_t referenceTimeItsMs,
    std::int64_t measurementTimeItsMs,
    std::int64_t firstObservedTimeItsMs)
{
    CpmSyntheticPerceptionScenario scenario;
    scenario.stationId = 9001;
    scenario.stationPosition = {100.0, 200.0, 10.0};
    scenario.referenceTimeItsMs = referenceTimeItsMs;
    scenario.knownObjectCount = 1;
    scenario.perceptionRangeMeters = 50.0;
    scenario.objects.push_back({
        601,
        {110.0, 205.0, 11.0},
        measurementTimeItsMs,
        firstObservedTimeItsMs
    });
    return scenario;
}

bool CheckDelayedMeasurementUsesPersistentHistory(
    std::int64_t firstObservedTimeItsMs)
{
    const auto scenario =
        MakeScenario(10000, 9990, firstObservedTimeItsMs);

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        accepted && error.empty() &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].objectId == 601 &&
        output.perceivedObjects[0].measurementDeltaTimeMs == -10 &&
        output.perceivedObjects[0].objectAgeMs.has_value() &&
        *output.perceivedObjects[0].objectAgeMs == 1000;

    return Check(
        correct,
        "DELAYED_MEASUREMENT_USES_TRACK_FIRST_OBSERVATION");
}

bool CheckPredictedStateUsesPersistentHistory(
    std::int64_t firstObservedTimeItsMs)
{
    const auto scenario =
        MakeScenario(10100, 10125, firstObservedTimeItsMs);

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        accepted && error.empty() &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].objectId == 601 &&
        output.perceivedObjects[0].measurementDeltaTimeMs == 25 &&
        output.perceivedObjects[0].objectAgeMs.has_value() &&
        *output.perceivedObjects[0].objectAgeMs == 1100;

    return Check(
        correct,
        "PREDICTED_STATE_PRESERVES_TRACK_FIRST_OBSERVATION");
}


bool CheckSyntheticSourceBuildWithPersistentHistory()
{
    CpmTrackHistory history;
    CpmInput output{};
    std::string error;

    // O valor 42 no campo firstObservedTimeItsMs é deliberadamente
    // incorreto para este histórico: a API integrada deverá usar o
    // instante observado pelo histórico persistente.
    auto firstScenario = MakeScenario(9000, 8990, 42);
    bool accepted = CpmSyntheticPerceptionSource::BuildWithHistory(
        firstScenario, 9000, history, output, error);

    bool correct =
        accepted && error.empty() &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].measurementDeltaTimeMs == -10 &&
        output.perceivedObjects[0].objectAgeMs.has_value() &&
        *output.perceivedObjects[0].objectAgeMs == 0;

    // A mesma pista é observada novamente 1000 ms depois.
    auto secondScenario = MakeScenario(10000, 9990, 42);
    error.clear();
    accepted = CpmSyntheticPerceptionSource::BuildWithHistory(
        secondScenario, 10000, history, output, error);

    correct = correct &&
        accepted && error.empty() &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].measurementDeltaTimeMs == -10 &&
        output.perceivedObjects[0].objectAgeMs.has_value() &&
        *output.perceivedObjects[0].objectAgeMs == 1000;

    // Um estado previsto para 10125 ms é incluído na mensagem com
    // referência 10100 ms; a idade continua a partir da primeira pista.
    auto predictedScenario = MakeScenario(10100, 10125, 42);
    error.clear();
    accepted = CpmSyntheticPerceptionSource::BuildWithHistory(
        predictedScenario, 10100, history, output, error);

    correct = correct &&
        accepted && error.empty() &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].measurementDeltaTimeMs == 25 &&
        output.perceivedObjects[0].objectAgeMs.has_value() &&
        *output.perceivedObjects[0].objectAgeMs == 1100;

    std::cout << (correct ? "PASS " : "FAIL ")
              << "SOURCE_USES_PERSISTENT_TRACK_HISTORY\n";

    // Uma construção inválida não deve comprometer o histórico: depois
    // da falha a referência 10125 ms ainda deve ser aceite, pois o último
    // instante confirmado continua a ser 10100 ms.
    auto invalidScenario = MakeScenario(10150, 10140, 42);
    invalidScenario.knownObjectCount = 0;

    CpmInput sentinel{};
    sentinel.station.stationId = 777;
    sentinel.referenceTimeItsMs = 333;
    sentinel.knownObjectCount = 1;

    error.clear();
    const bool invalidAccepted =
        CpmSyntheticPerceptionSource::BuildWithHistory(
            invalidScenario, 10150, history, sentinel, error);
    const bool invalidErrorPresent = !error.empty();

    const bool outputPreserved =
        sentinel.station.stationId == 777 &&
        sentinel.referenceTimeItsMs == 333 &&
        sentinel.knownObjectCount == 1 &&
        sentinel.perceivedObjects.empty();

    auto recoveryScenario = MakeScenario(10125, 10115, 42);
    output = CpmInput{};
    error.clear();

    const bool recovered =
        CpmSyntheticPerceptionSource::BuildWithHistory(
            recoveryScenario, 10125, history, output, error);

    const bool recoveryCorrect =
        recovered && error.empty() &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].measurementDeltaTimeMs == -10 &&
        output.perceivedObjects[0].objectAgeMs.has_value() &&
        *output.perceivedObjects[0].objectAgeMs == 1125;

    const bool atomic =
        !invalidAccepted && invalidErrorPresent &&
        outputPreserved && recoveryCorrect;

    std::cout << (atomic ? "PASS " : "FAIL ")
              << "SOURCE_HISTORY_UPDATE_IS_ATOMIC\n";

    return correct && atomic;
}


} // namespace

int main()
{
    bool passed = true;
    CpmTrackHistory history;
    std::string error = "stale error";
    std::int64_t firstObserved = -99;

    const bool firstAccepted =
        history.Observe(601, 9000, firstObserved, error);

    passed = Check(
        firstAccepted && firstObserved == 9000 && error.empty(),
        "FIRST_OBSERVATION_INITIALIZES_TRACK") && passed;

    error = "stale error";
    const bool secondAccepted =
        history.Observe(601, 10000, firstObserved, error);

    passed = Check(
        secondAccepted && firstObserved == 9000 && error.empty(),
        "REPEATED_OBSERVATION_PRESERVES_FIRST_SEEN_TIME") && passed;

    passed = CheckDelayedMeasurementUsesPersistentHistory(
        firstObserved) && passed;

    error = "stale error";
    const bool thirdAccepted =
        history.Observe(601, 10100, firstObserved, error);

    passed = Check(
        thirdAccepted && firstObserved == 9000 && error.empty(),
        "CONTINUOUS_TRACK_RETAINS_FIRST_SEEN_TIME") && passed;

    passed = CheckPredictedStateUsesPersistentHistory(
        firstObserved) && passed;

    std::int64_t independentFirstSeen = -99;
    error = "stale error";
    const bool independentAccepted =
        history.Observe(602, 10100, independentFirstSeen, error);

    passed = Check(
        independentAccepted &&
        independentFirstSeen == 10100 &&
        error.empty(),
        "TRACKS_KEEP_INDEPENDENT_FIRST_SEEN_TIMES") && passed;

    // Uma regressão temporal não pode alterar nem a saída nem o histórico.
    firstObserved = 7777;
    error = "stale error";
    const bool regressingAccepted =
        history.Observe(601, 10099, firstObserved, error);

    passed = Check(
        !regressingAccepted &&
        firstObserved == 7777 &&
        !error.empty(),
        "REGRESSING_TIMESTAMP_REJECTED_ATOMICALLY") && passed;

    firstObserved = -99;
    error.clear();
    const bool recoveryAccepted =
        history.Observe(601, 10110, firstObserved, error);

    passed = Check(
        recoveryAccepted &&
        firstObserved == 9000 &&
        error.empty(),
        "VALID_OBSERVATION_PRESERVES_HISTORY_AFTER_REJECTION") && passed;

    firstObserved = 7777;
    error = "stale error";
    const bool zeroIdAccepted =
        history.Observe(0, 10120, firstObserved, error);

    passed = Check(
        !zeroIdAccepted &&
        firstObserved == 7777 &&
        !error.empty(),
        "ZERO_OBJECT_ID_REJECTED_ATOMICALLY") && passed;

    firstObserved = 7777;
    error = "stale error";
    const bool negativeTimeAccepted =
        history.Observe(603, -1, firstObserved, error);

    passed = Check(
        !negativeTimeAccepted &&
        firstObserved == 7777 &&
        !error.empty(),
        "NEGATIVE_OBSERVATION_TIME_REJECTED_ATOMICALLY") && passed;

    error = "stale error";
    const bool forgotten =
        history.Forget(601, error);

    passed = Check(
        forgotten && error.empty(),
        "TRACK_CAN_BE_EXPLICITLY_FORGOTTEN") && passed;

    firstObserved = -99;
    error = "stale error";
    const bool restartedAccepted =
        history.Observe(601, 10200, firstObserved, error);

    passed = Check(
        restartedAccepted &&
        firstObserved == 10200 &&
        error.empty(),
        "REOBSERVED_TRACK_STARTS_NEW_HISTORY") && passed;

    passed = CheckSyntheticSourceBuildWithPersistentHistory() && passed;

    std::cout << (passed
        ? "CPM_TRACK_HISTORY=PASS\n"
        : "CPM_TRACK_HISTORY=FAIL\n");

    return passed ? 0 : 1;
}
