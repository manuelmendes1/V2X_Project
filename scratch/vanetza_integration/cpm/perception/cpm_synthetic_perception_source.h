#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_SYNTHETIC_PERCEPTION_SOURCE_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_SYNTHETIC_PERCEPTION_SOURCE_H

#include <cstdint>
#include <string>
#include <vector>

#include "vanetza_integration/cpm/perception/cpm_coordinate_transform.h"
#include "vanetza_integration/cpm/perception/cpm_track_history.h"
#include "vanetza_integration/cpm/types/cpm_input.h"

struct CpmSyntheticObjectObservation
{
    std::uint16_t objectId{};
    CpmLocalPosition position{};
    // Instante ITS a que o estado descrito pelo objeto se refere:
    // aquisição observada ou instante-alvo de um estado predito, em ms.
    std::int64_t measurementTimeItsMs{};

    // Instante ITS da primeira observação/conhecimento do objeto pela
    // estação emissora, em ms. Não é o instante da última medição.
    std::int64_t firstObservedTimeItsMs{};
};

struct CpmSyntheticPerceptionScenario
{
    std::uint32_t stationId{};
    CpmLocalPosition stationPosition{};
    std::int64_t referenceTimeItsMs{};
    // Alcance horizontal de perceção, em metros. Objetos à distância
    // exatamente igual ao limite são incluídos. O valor tem de ser finito
    // e não negativo. Valor predefinido para cenários reproduzíveis.
    double perceptionRangeMeters{50.0};
    std::uint8_t knownObjectCount{};
    std::vector<CpmSyntheticObjectObservation> objects;
};

class CpmSyntheticPerceptionSource
{
public:
    // Produz um CpmInput determinístico a partir de observações explícitas.
    //
    // Esta fonte sintética representa uma RSU: as coordenadas locais são
    // interpretadas como ENU (X=Este, Y=Norte, Z=Cima), em metros.
    // Não se presume uma projeção WGS84 real nem se fabricam coordenadas
    // geográficas para a posição de referência da estação.
    //
    // knownObjectCount representa o total conhecido no LDM e não deve ser
    // substituído automaticamente pelo número de objetos incluídos.
    //
    // Em caso de erro, output permanece inalterado.
    static bool Build(
        const CpmSyntheticPerceptionScenario& scenario,
        CpmInput& output,
        std::string& error);

    // Constrói a mensagem usando histórico persistente por objectId.
    // observationTimeItsMs é o instante em que a estação observou
    // os objetos fornecidos neste ciclo; não é o instante da medição.
    //
    // O histórico é atualizado transacionalmente: se a validação da
    // observação ou a construção da mensagem falhar, history e output
    // permanecem inalterados.
    static bool BuildWithHistory(
        const CpmSyntheticPerceptionScenario& scenario,
        std::int64_t observationTimeItsMs,
        CpmTrackHistory& history,
        CpmInput& output,
        std::string& error);
};

#endif
