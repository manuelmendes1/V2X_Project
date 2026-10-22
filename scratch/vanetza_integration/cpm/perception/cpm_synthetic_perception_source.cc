#include "vanetza_integration/cpm/perception/cpm_synthetic_perception_source.h"

#include <cmath>
#include <cstdint>
#include <utility>
#include <unordered_set>

#include "vanetza_integration/cpm/perception/cpm_temporal_transform.h"

namespace
{

bool IsFinitePosition(const CpmLocalPosition& position)
{
    if (!std::isfinite(position.xMeters) ||
        !std::isfinite(position.yMeters))
    {
        return false;
    }

    return !position.zMeters.has_value() ||
           std::isfinite(*position.zMeters);
}

bool Reject(std::string& error, const std::string& reason)
{
    error = reason;
    return false;
}

} // namespace

bool
CpmSyntheticPerceptionSource::Build(
    const CpmSyntheticPerceptionScenario& scenario,
    CpmInput& output,
    std::string& error)
{
    if (scenario.stationId == 0)
    {
        return Reject(error, "CPM stationId must be non-zero");
    }

    if (!IsFinitePosition(scenario.stationPosition))
    {
        return Reject(
            error,
            "CPM station position must contain only finite coordinates");
    }

    if (!std::isfinite(scenario.perceptionRangeMeters) ||
        scenario.perceptionRangeMeters < 0.0)
    {
        return Reject(
            error,
            "Perception range must be finite and non-negative");
    }

    if (scenario.referenceTimeItsMs < 0 ||
        scenario.referenceTimeItsMs > 4398046511103LL)
    {
        return Reject(
            error,
            "CPM referenceTime is outside the TimestampIts range");
    }

    // Build into a temporary value so any failed validation leaves output
    // unchanged. The local scenario coordinates are not WGS84 coordinates.
    CpmInput candidate{};
    candidate.station.stationId = scenario.stationId;
    // O referencial dos objetos desta fonte é ENU, próprio do cenário RSU sintético.
    candidate.station.origin = CpmRoadSideUnitStationData{};
    candidate.referenceTimeItsMs = scenario.referenceTimeItsMs;
    candidate.knownObjectCount = scenario.knownObjectCount;

    std::unordered_set<std::uint16_t> seenObjectIds;
    seenObjectIds.reserve(scenario.objects.size());
    candidate.perceivedObjects.reserve(scenario.objects.size());

    for (std::size_t index = 0;
         index < scenario.objects.size();
         ++index)
    {
        const auto& observation = scenario.objects[index];

        if (observation.objectId == 0)
        {
            return Reject(
                error,
                "Object ID must be non-zero at observation index " +
                    std::to_string(index));
        }

        if (!seenObjectIds.insert(observation.objectId).second)
        {
            return Reject(
                error,
                "Duplicate object ID " +
                    std::to_string(observation.objectId));
        }

        CpmCartesianPosition3D relativePosition{};
        std::string transformError;

        if (!CpmCoordinateTransform::ToEnuRelative(
                scenario.stationPosition,
                observation.position,
                relativePosition,
                transformError))
        {
            return Reject(
                error,
                "Invalid coordinates for object " +
                    std::to_string(observation.objectId) + ": " +
                    transformError);
        }

        // O alcance é horizontal: não inclui o delta vertical.
        // O limite é inclusivo para que um objeto exatamente na fronteira
        // pertença ao conjunto de objetos incluídos.
        const double horizontalDistance = std::hypot(
            relativePosition.xEastMeters,
            relativePosition.yNorthMeters);

        if (horizontalDistance > scenario.perceptionRangeMeters)
        {
            continue;
        }

        std::int32_t measurementDeltaMs = 0;
        std::string temporalError;

        if (!CpmTemporalTransform::ComputeMeasurementDeltaTimeMs(
                scenario.referenceTimeItsMs,
                observation.measurementTimeItsMs,
                measurementDeltaMs,
                temporalError))
        {
            return Reject(
                error,
                "Invalid measurement time for object " +
                    std::to_string(observation.objectId) + ": " +
                    temporalError);
        }

        std::int32_t objectAgeMs = 0;
        temporalError.clear();

        if (!CpmTemporalTransform::ComputeObjectAgeMs(
                scenario.referenceTimeItsMs,
                observation.firstObservedTimeItsMs,
                objectAgeMs,
                temporalError))
        {
            return Reject(
                error,
                "Invalid first-observed time for object " +
                    std::to_string(observation.objectId) + ": " +
                    temporalError);
        }

        CpmPerceivedObject perceivedObject{};
        perceivedObject.objectId = observation.objectId;
        perceivedObject.measurementDeltaTimeMs = measurementDeltaMs;
        perceivedObject.objectAgeMs = objectAgeMs;

        perceivedObject.position.xEast = {
            relativePosition.xEastMeters,
            std::nullopt};

        perceivedObject.position.yNorth = {
            relativePosition.yNorthMeters,
            std::nullopt};

        if (relativePosition.zUpMeters.has_value())
        {
            perceivedObject.position.zUp = CpmCoordinateWithConfidence{
                *relativePosition.zUpMeters,
                std::nullopt};
        }

        candidate.perceivedObjects.push_back(
            std::move(perceivedObject));
    }

    // Validar a contagem depois do filtro espacial. knownObjectCount
    // representa o total conhecido na LDM, não o total transmitido.
    if (candidate.perceivedObjects.size() > scenario.knownObjectCount)
    {
        return Reject(
            error,
            "knownObjectCount is below the number of included objects");
    }

    // No valid geodetic origin is provided by this synthetic scenario.
    // Keep latitude, longitude and altitude unavailable rather than
    // manufacturing WGS84 values from local Cartesian coordinates.
    output = std::move(candidate);
    error.clear();
    return true;
}

bool
CpmSyntheticPerceptionSource::BuildWithHistory(
    const CpmSyntheticPerceptionScenario& scenario,
    std::int64_t observationTimeItsMs,
    CpmTrackHistory& history,
    CpmInput& output,
    std::string& error)
{
    // Trabalhar sobre cópias impede que uma mensagem inválida avance
    // o histórico ou altere o output visível ao chamador.
    CpmTrackHistory candidateHistory = history;
    CpmSyntheticPerceptionScenario candidateScenario = scenario;

    for (std::size_t index = 0;
         index < candidateScenario.objects.size();
         ++index)
    {
        auto& observation = candidateScenario.objects[index];

        std::int64_t firstObservedTimeItsMs = 0;
        std::string historyError;

        if (!candidateHistory.Observe(
                observation.objectId,
                observationTimeItsMs,
                firstObservedTimeItsMs,
                historyError))
        {
            return Reject(
                error,
                "Cannot update track history for object " +
                    std::to_string(observation.objectId) + ": " +
                    historyError);
        }

        // O instante inicial vem do histórico autoritativo, não do valor
        // fornecido pelo cenário de entrada.
        observation.firstObservedTimeItsMs =
            firstObservedTimeItsMs;
    }

    CpmInput candidateOutput{};

    if (!Build(candidateScenario, candidateOutput, error))
    {
        // candidateHistory é descartado: não confirmar atualizações
        // de pistas quando a mensagem não pode ser construída.
        return false;
    }

    history = std::move(candidateHistory);
    output = std::move(candidateOutput);
    error.clear();
    return true;
}
