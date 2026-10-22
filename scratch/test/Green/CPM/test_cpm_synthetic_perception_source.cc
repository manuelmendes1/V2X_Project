#include "vanetza_integration/cpm/perception/cpm_synthetic_perception_source.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <variant>

namespace
{

bool Near(double actual, double expected)
{
    return std::isfinite(actual) &&
           std::abs(actual - expected) <= 1e-9;
}

CpmSyntheticPerceptionScenario BuildKnownScenario()
{
    CpmSyntheticPerceptionScenario scenario;
    scenario.stationId = 9001;
    scenario.stationPosition = {100.0, 200.0, 10.0};
    scenario.referenceTimeItsMs = 10000;
    scenario.knownObjectCount = 5;
    scenario.perceptionRangeMeters = 50.0;

    scenario.objects.push_back({
        501,
        {112.0, 194.0, 12.0},
        9988,
        9000
    });

    scenario.objects.push_back({
        502,
        {95.0, 215.0, std::nullopt},
        10025,
        10000
    });

    return scenario;
}


CpmSyntheticPerceptionScenario BuildTrackedObjectScenario(
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

bool CheckKnownScenario()
{
    const auto scenario = BuildKnownScenario();

    CpmInput output{};
    std::string error = "stale error";

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    bool correct =
        accepted &&
        error.empty() &&
        output.station.stationId == 9001 &&
        std::holds_alternative<CpmRoadSideUnitStationData>(
            output.station.origin) &&
        output.referenceTimeItsMs == 10000 &&
        output.knownObjectCount == 5 &&
        output.perceivedObjects.size() == 2 &&
        !output.referencePosition.latitudeDeg.has_value() &&
        !output.referencePosition.longitudeDeg.has_value() &&
        !output.referencePosition.altitudeMeters.has_value();

    if (correct)
    {
        const auto& first = output.perceivedObjects[0];
        const auto& second = output.perceivedObjects[1];

        correct =
            first.objectId == 501 &&
            first.measurementDeltaTimeMs == -12 &&
            first.objectAgeMs == 1000 &&
            Near(first.position.xEast.value, 12.0) &&
            Near(first.position.yNorth.value, -6.0) &&
            first.position.zUp.has_value() &&
            Near(first.position.zUp->value, 2.0) &&
            second.objectId == 502 &&
            second.measurementDeltaTimeMs == 25 &&
            second.objectAgeMs == 0 &&
            Near(second.position.xEast.value, -5.0) &&
            Near(second.position.yNorth.value, 15.0) &&
            !second.position.zUp.has_value();
    }

    std::cout << (correct ? "PASS " : "FAIL ")
              << "SYNTHETIC_KNOWN_SCENARIO\n";

    if (!correct)
    {
        std::cerr
            << "  accepted=" << accepted
            << " error=" << error
            << " stationId=" << output.station.stationId
            << " referenceTime=" << output.referenceTimeItsMs
            << " knownObjectCount="
            << static_cast<unsigned>(output.knownObjectCount)
            << " includedObjects=" << output.perceivedObjects.size()
            << '\n';
    }

    return correct;
}

bool CheckContinuousTrackAgeAdvances()
{
    // O mesmo objeto continua conhecido pela mesma estação.
    // A medição permanece 10 ms atrasada; a idade decorre da primeira
    // observação e aumenta com o tempo da mensagem.
    const auto firstScenario =
        BuildTrackedObjectScenario(10000, 9990, 9000);
    const auto secondScenario =
        BuildTrackedObjectScenario(10100, 10090, 9000);

    CpmInput firstOutput{};
    CpmInput secondOutput{};
    std::string firstError;
    std::string secondError;

    const bool firstAccepted =
        CpmSyntheticPerceptionSource::Build(
            firstScenario, firstOutput, firstError);
    const bool secondAccepted =
        CpmSyntheticPerceptionSource::Build(
            secondScenario, secondOutput, secondError);

    bool correct =
        firstAccepted && secondAccepted &&
        firstError.empty() && secondError.empty() &&
        firstOutput.perceivedObjects.size() == 1 &&
        secondOutput.perceivedObjects.size() == 1;

    if (correct)
    {
        const auto& first = firstOutput.perceivedObjects.front();
        const auto& second = secondOutput.perceivedObjects.front();

        correct =
            first.objectId == 601 &&
            second.objectId == first.objectId &&
            first.measurementDeltaTimeMs == -10 &&
            second.measurementDeltaTimeMs == -10 &&
            first.objectAgeMs.has_value() &&
            second.objectAgeMs.has_value() &&
            *first.objectAgeMs == 1000 &&
            *second.objectAgeMs == 1100;
    }

    std::cout << (correct ? "PASS " : "FAIL ")
              << "CONTINUOUS_TRACK_AGE_ADVANCES_INDEPENDENTLY\n";
    return correct;
}

bool CheckPredictedStateHasPositiveDeltaAndContinuousAge()
{
    // A referência é 10100 ms e o estado representado prevê o objeto
    // para 10125 ms. A primeira observação continua a ser 9000 ms.
    const auto scenario =
        BuildTrackedObjectScenario(10100, 10125, 9000);

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    bool correct =
        accepted && error.empty() &&
        output.referenceTimeItsMs == 10100 &&
        output.perceivedObjects.size() == 1;

    if (correct)
    {
        const auto& object = output.perceivedObjects.front();
        correct =
            object.objectId == 601 &&
            object.measurementDeltaTimeMs == 25 &&
            object.objectAgeMs.has_value() &&
            *object.objectAgeMs == 1100;
    }

    std::cout << (correct ? "PASS " : "FAIL ")
              << "PREDICTED_STATE_POSITIVE_DELTA_PRESERVES_AGE\n";
    return correct;
}

bool CheckDuplicateObjectIdsRejected()
{
    auto scenario = BuildKnownScenario();
    scenario.objects[1].objectId = scenario.objects[0].objectId;

    CpmInput output{};
    output.station.stationId = 77;
    output.referenceTimeItsMs = 1234;
    output.knownObjectCount = 2;

    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        !accepted &&
        !error.empty() &&
        output.station.stationId == 77 &&
        output.referenceTimeItsMs == 1234 &&
        output.knownObjectCount == 2 &&
        output.perceivedObjects.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << "DUPLICATE_OBJECT_IDS_REJECTED\n";

    return correct;
}

bool CheckImpossibleObjectAgeRejectedAtomically()
{
    auto scenario = BuildKnownScenario();
    scenario.objects[0].firstObservedTimeItsMs = 10001;

    CpmInput output{};
    output.station.stationId = 77;
    output.referenceTimeItsMs = 1234;
    output.knownObjectCount = 2;

    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        !accepted &&
        !error.empty() &&
        output.station.stationId == 77 &&
        output.referenceTimeItsMs == 1234 &&
        output.knownObjectCount == 2 &&
        output.perceivedObjects.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << "INVALID_TIME_REJECTED_ATOMICALLY\n";

    return correct;
}

bool CheckKnownCountBelowIncludedObjectsRejected()
{
    auto scenario = BuildKnownScenario();
    scenario.knownObjectCount = 1;

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct = !accepted && !error.empty() &&
                         output.perceivedObjects.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << "KNOWN_COUNT_BELOW_INCLUDED_REJECTED\n";

    return correct;
}

bool CheckZeroObjectIdRejected()
{
    auto scenario = BuildKnownScenario();
    scenario.objects[0].objectId = 0;

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct = !accepted && !error.empty() &&
                         output.perceivedObjects.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << "ZERO_OBJECT_ID_REJECTED\n";

    return correct;
}


bool CheckPerceptionRangeAndKnownCount()
{
    auto scenario = BuildKnownScenario();
    scenario.perceptionRangeMeters = 16.0;

    // Distância horizontal exatamente igual ao alcance: incluir.
    scenario.objects.push_back({
        504,
        {116.0, 200.0, std::nullopt},
        10010,
        10000
    });

    // Distância horizontal superior ao alcance: excluir.
    scenario.objects.push_back({
        505,
        {116.0001, 200.0, std::nullopt},
        10010,
        10000
    });

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        accepted &&
        error.empty() &&
        output.knownObjectCount == 5 &&
        output.perceivedObjects.size() == 3 &&
        output.perceivedObjects[0].objectId == 501 &&
        output.perceivedObjects[1].objectId == 502 &&
        output.perceivedObjects[2].objectId == 504;

    std::cout << (correct ? "PASS " : "FAIL ")
              << "PERCEPTION_RANGE_AND_KNOWN_COUNT\n";

    if (!correct)
    {
        std::cerr
            << "  accepted=" << accepted
            << " error=" << error
            << " knownObjectCount="
            << static_cast<unsigned>(output.knownObjectCount)
            << " included=" << output.perceivedObjects.size()
            << '\n';
    }

    return correct;
}

bool CheckZeroRangeIncludesOnlyCoincidentObjects()
{
    auto scenario = BuildKnownScenario();
    scenario.perceptionRangeMeters = 0.0;

    scenario.objects.push_back({
        503,
        {100.0, 200.0, std::nullopt},
        10000,
        10000
    });

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        accepted &&
        error.empty() &&
        output.knownObjectCount == 5 &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].objectId == 503;

    std::cout << (correct ? "PASS " : "FAIL ")
              << "ZERO_RANGE\n";

    return correct;
}

bool CheckNegativeRangeRejectedAtomically()
{
    auto scenario = BuildKnownScenario();
    scenario.perceptionRangeMeters = -1.0;

    CpmInput output{};
    output.station.stationId = 77;
    output.referenceTimeItsMs = 1234;
    output.knownObjectCount = 2;
    output.perceivedObjects.emplace_back();
    output.perceivedObjects.back().objectId = 99;

    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        !accepted &&
        !error.empty() &&
        output.station.stationId == 77 &&
        output.referenceTimeItsMs == 1234 &&
        output.knownObjectCount == 2 &&
        output.perceivedObjects.size() == 1 &&
        output.perceivedObjects[0].objectId == 99;

    std::cout << (correct ? "PASS " : "FAIL ")
              << "NEGATIVE_RANGE_REJECTED_ATOMICALLY\n";

    return correct;
}

bool CheckNonFiniteRangeRejectedAtomically()
{
    auto scenario = BuildKnownScenario();
    scenario.perceptionRangeMeters =
        std::numeric_limits<double>::infinity();

    CpmInput output{};
    output.station.stationId = 88;
    output.referenceTimeItsMs = 5678;

    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        !accepted &&
        !error.empty() &&
        output.station.stationId == 88 &&
        output.referenceTimeItsMs == 5678;

    std::cout << (correct ? "PASS " : "FAIL ")
              << "NON_FINITE_RANGE_REJECTED_ATOMICALLY\n";

    return correct;
}

bool CheckKnownCountBelowFilteredIncludedCountRejected()
{
    auto scenario = BuildKnownScenario();
    scenario.perceptionRangeMeters = 16.0;
    scenario.knownObjectCount = 2;

    // A estação e os dois objetos originais ficam dentro do alcance.
    // A construção deve rejeitar a mensagem porque incluiria três objetos,
    // embora o total declarado na LDM seja apenas dois.
    scenario.objects.push_back({
        504,
        {116.0, 200.0, std::nullopt},
        10010,
        10000
    });

    CpmInput output{};
    std::string error;

    const bool accepted =
        CpmSyntheticPerceptionSource::Build(
            scenario, output, error);

    const bool correct =
        !accepted &&
        !error.empty() &&
        output.perceivedObjects.empty();

    std::cout << (correct ? "PASS " : "FAIL ")
              << "KNOWN_COUNT_BELOW_FILTERED_INCLUDED_REJECTED\n";

    return correct;
}

} // namespace

int main()
{
    bool passed = true;

    passed = CheckKnownScenario() && passed;
    passed = CheckContinuousTrackAgeAdvances() && passed;
    passed = CheckPredictedStateHasPositiveDeltaAndContinuousAge() && passed;
    passed = CheckDuplicateObjectIdsRejected() && passed;
    passed = CheckImpossibleObjectAgeRejectedAtomically() && passed;
    passed = CheckKnownCountBelowIncludedObjectsRejected() && passed;
    passed = CheckZeroObjectIdRejected() && passed;
    passed = CheckPerceptionRangeAndKnownCount() && passed;
    passed = CheckZeroRangeIncludesOnlyCoincidentObjects() && passed;
    passed = CheckNegativeRangeRejectedAtomically() && passed;
    passed = CheckNonFiniteRangeRejectedAtomically() && passed;
    passed = CheckKnownCountBelowFilteredIncludedCountRejected() && passed;

    if (!passed)
    {
        return 1;
    }

    std::cout << "CPM_SYNTHETIC_PERCEPTION_SOURCE=PASS\n";
    return 0;
}
