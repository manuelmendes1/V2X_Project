#include "vanetza_integration/cpm/perception/cpm_vehicle_perception_source.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

#include "vanetza_integration/cpm/perception/cpm_temporal_transform.h"

namespace
{

bool Reject(std::string& error, const std::string& reason)
{
    error = reason;
    return false;
}

bool IsValidHorizontalPosition(
    const CpmGeodeticHorizontalPosition& position)
{
    return std::isfinite(position.latitudeDeg) &&
           std::isfinite(position.longitudeDeg) &&
           position.latitudeDeg >= -90.0 &&
           position.latitudeDeg <= 90.0 &&
           position.longitudeDeg >= -180.0 &&
           position.longitudeDeg <= 180.0;
}

} // namespace

bool
CpmVehiclePerceptionSource::Build(
    const CpmVehiclePerceptionConfig& config,
    std::int64_t referenceSimulationTimeUs,
    const std::vector<CpmVehiclePerceptionObservation>& observations,
    CpmInput& output,
    std::string& error)
{
    if (config.stationId == 0)
    {
        return Reject(error, "CPM RSU stationId must be non-zero");
    }

    const CpmGeodeticHorizontalPosition stationPosition{
        config.stationLatitudeDeg,
        config.stationLongitudeDeg};

    if (!IsValidHorizontalPosition(stationPosition))
    {
        return Reject(error, "CPM RSU horizontal position is invalid");
    }

    if (!std::isfinite(config.perceptionRangeMeters) ||
        config.perceptionRangeMeters < 0.0)
    {
        return Reject(
            error,
            "CPM perception range must be finite and non-negative");
    }

    if (observations.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint8_t>::max()))
    {
        return Reject(
            error,
            "Known object count exceeds the CPM model range");
    }

    std::int64_t referenceTimeItsMs = 0;
    std::string temporalError;

    if (!CpmTemporalTransform::BuildReferenceTime(
            config.itsStartClockUs,
            referenceSimulationTimeUs,
            referenceTimeItsMs,
            temporalError))
    {
        return Reject(
            error,
            "Cannot build CPM referenceTime: " + temporalError);
    }

    // Validar todas as identidades e todos os valores geográficos fornecidos
    // antes de criar o estado candidato.
    std::unordered_set<std::string> seenVehicleIds;
    seenVehicleIds.reserve(observations.size());

    for (std::size_t index = 0; index < observations.size(); ++index)
    {
        const auto& observation = observations[index];

        if (observation.vehicleId.empty())
        {
            return Reject(
                error,
                "Vehicle ID is empty at observation index " +
                    std::to_string(index));
        }

        if (!seenVehicleIds.insert(observation.vehicleId).second)
        {
            return Reject(
                error,
                "Duplicate vehicle ID: " + observation.vehicleId);
        }

        if (observation.position.has_value() &&
            !IsValidHorizontalPosition(*observation.position))
        {
            return Reject(
                error,
                "Invalid WGS84 position for vehicle " +
                    observation.vehicleId);
        }

        if (observation.sumoSimulationTimeUs.has_value() &&
            *observation.sumoSimulationTimeUs >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
        {
            return Reject(
                error,
                "SUMO timestamp exceeds the supported int64 range for " +
                    observation.vehicleId);
        }
    }

    // Trabalhar sobre cópias garante rollback completo se qualquer veículo
    // ou timestamp falhar durante a construção.
    auto candidateObjectIds = m_vehicleObjectIds;
    auto candidateHistory = m_trackHistory;
    std::uint32_t candidateNextObjectId = m_nextObjectId;

    for (const auto& observation : observations)
    {
        if (candidateObjectIds.find(observation.vehicleId) !=
            candidateObjectIds.end())
        {
            continue;
        }

        if (candidateNextObjectId >
            std::numeric_limits<std::uint16_t>::max())
        {
            return Reject(
                error,
                "Exhausted the local CPM object identifier range");
        }

        const auto objectId =
            static_cast<std::uint16_t>(candidateNextObjectId);

        candidateObjectIds.emplace(observation.vehicleId, objectId);
        ++candidateNextObjectId;
    }

    CpmInput candidate{};
    candidate.station.stationId = config.stationId;
    candidate.station.origin = CpmRoadSideUnitStationData{};
    candidate.referenceTimeItsMs = referenceTimeItsMs;
    candidate.referencePosition.latitudeDeg = config.stationLatitudeDeg;
    candidate.referencePosition.longitudeDeg = config.stationLongitudeDeg;
    candidate.referencePosition.altitudeMeters = std::nullopt;
    candidate.knownObjectCount =
        static_cast<std::uint8_t>(observations.size());
    candidate.perceivedObjects.reserve(observations.size());

    for (const auto& observation : observations)
    {
        // Sem posição ou timestamp não se pode construir um objeto CPM
        // coerente. A observação continua contabilizada em knownObjectCount.
        if (!observation.position.has_value() ||
            !observation.sumoSimulationTimeUs.has_value())
        {
            continue;
        }

        std::int64_t measurementTimeItsMs = 0;
        temporalError.clear();

        if (!CpmTemporalTransform::BuildReferenceTime(
                config.itsStartClockUs,
                static_cast<std::int64_t>(
                    *observation.sumoSimulationTimeUs),
                measurementTimeItsMs,
                temporalError))
        {
            return Reject(
                error,
                "Invalid SUMO measurement time for vehicle " +
                    observation.vehicleId + ": " + temporalError);
        }

        CpmCartesianPosition3D relativePosition{};
        std::string transformError;

        if (!CpmGeodeticTransform::ToEnuHorizontalRelative(
                stationPosition,
                *observation.position,
                relativePosition,
                transformError))
        {
            return Reject(
                error,
                "Cannot transform vehicle " + observation.vehicleId +
                    " to horizontal ENU: " + transformError);
        }

        const double horizontalDistance = std::hypot(
            relativePosition.xEastMeters,
            relativePosition.yNorthMeters);

        if (!std::isfinite(horizontalDistance))
        {
            return Reject(
                error,
                "Horizontal distance is not finite for vehicle " +
                    observation.vehicleId);
        }

        const auto idFound =
            candidateObjectIds.find(observation.vehicleId);

        if (idFound == candidateObjectIds.end())
        {
            return Reject(
                error,
                "Missing CPM object ID for vehicle " +
                    observation.vehicleId);
        }

        std::int64_t firstObservedTimeItsMs = 0;
        std::string historyError;

        if (!candidateHistory.Observe(
                idFound->second,
                referenceTimeItsMs,
                firstObservedTimeItsMs,
                historyError))
        {
            return Reject(
                error,
                "Cannot update CPM history for vehicle " +
                    observation.vehicleId + ": " + historyError);
        }

        // O limite de perceção é horizontal e inclusivo.
        if (horizontalDistance > config.perceptionRangeMeters)
        {
            continue;
        }

        std::int32_t measurementDeltaMs = 0;
        temporalError.clear();

        if (!CpmTemporalTransform::ComputeMeasurementDeltaTimeMs(
                referenceTimeItsMs,
                measurementTimeItsMs,
                measurementDeltaMs,
                temporalError))
        {
            return Reject(
                error,
                "Invalid measurement delta for vehicle " +
                    observation.vehicleId + ": " + temporalError);
        }

        std::int32_t objectAgeMs = 0;
        temporalError.clear();

        if (!CpmTemporalTransform::ComputeObjectAgeMs(
                referenceTimeItsMs,
                firstObservedTimeItsMs,
                objectAgeMs,
                temporalError))
        {
            return Reject(
                error,
                "Invalid object age for vehicle " +
                    observation.vehicleId + ": " + temporalError);
        }

        CpmPerceivedObject perceivedObject{};
        perceivedObject.objectId = idFound->second;
        perceivedObject.measurementDeltaTimeMs = measurementDeltaMs;
        perceivedObject.objectAgeMs = objectAgeMs;

        perceivedObject.position.xEast = {
            relativePosition.xEastMeters,
            std::nullopt};

        perceivedObject.position.yNorth = {
            relativePosition.yNorthMeters,
            std::nullopt};

        // A altitude não foi fornecida; zUp permanece indisponível.
        candidate.perceivedObjects.push_back(
            std::move(perceivedObject));
    }

    // Confirmar output e estado interno apenas depois de todas as validações.
    m_vehicleObjectIds = std::move(candidateObjectIds);
    m_trackHistory = std::move(candidateHistory);
    m_nextObjectId = candidateNextObjectId;
    output = std::move(candidate);
    error.clear();
    return true;
}
