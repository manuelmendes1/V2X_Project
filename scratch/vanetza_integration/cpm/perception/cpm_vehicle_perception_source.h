#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_VEHICLE_PERCEPTION_SOURCE_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_VEHICLE_PERCEPTION_SOURCE_H

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "vanetza_integration/cpm/perception/cpm_geodetic_transform.h"
#include "vanetza_integration/cpm/perception/cpm_track_history.h"
#include "vanetza_integration/cpm/types/cpm_input.h"

struct CpmVehiclePerceptionConfig
{
    std::uint32_t stationId{};
    double stationLatitudeDeg{};
    double stationLongitudeDeg{};
    double perceptionRangeMeters{50.0};

    // time_since_epoch().count() de vanetza::Clock::at(itsStartTime),
    // em microssegundos desde 2004-01-01 00:00:00 UTC.
    std::int64_t itsStartClockUs{};
};

struct CpmVehiclePerceptionObservation
{
    std::string vehicleId;

    // Ausente se a amostra não possuir coordenadas WGS84 válidas.
    std::optional<CpmGeodeticHorizontalPosition> position;

    // Timestamp lógico SUMO em microssegundos desde o início da simulação.
    // Ausente para datagramas antigos ou sem timestamp.
    std::optional<std::uint64_t> sumoSimulationTimeUs;
};

class CpmVehiclePerceptionSource
{
public:
    // Constrói um CpmInput a partir das observações de veículos fornecidas.
    //
    // referenceSimulationTimeUs é o tempo lógico SUMO/ns-3 do ciclo atual.
    // itsStartClockUs estabelece a origem do domínio temporal ITS.
    //
    // O identificador CPM do objeto é local à estação e permanece estável
    // enquanto esta instância da fonte mantiver o seu estado.
    //
    // Objetos sem posição geográfica ou timestamp são contabilizados em
    // knownObjectCount, mas não são incluídos em perceivedObjects.
    //
    // A altura da estação e dos veículos não é inferida. A posição CPM da
    // estação não contém altitude conhecida e os objetos só incluem ENU
    // horizontal.
    //
    // A construção é transacional: se houver erro, output e o estado interno
    // desta fonte permanecem inalterados.
    bool Build(
        const CpmVehiclePerceptionConfig& config,
        std::int64_t referenceSimulationTimeUs,
        const std::vector<CpmVehiclePerceptionObservation>& observations,
        CpmInput& output,
        std::string& error);

private:
    std::unordered_map<std::string, std::uint16_t> m_vehicleObjectIds;
    CpmTrackHistory m_trackHistory;
    std::uint32_t m_nextObjectId{1};
};

#endif
