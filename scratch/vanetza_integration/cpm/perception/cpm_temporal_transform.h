#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_TEMPORAL_TRANSFORM_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_TEMPORAL_TRANSFORM_H

#include <cstdint>
#include <string>

class CpmTemporalTransform
{
public:
    // itsStartClockUs:
    //   time_since_epoch().count() de vanetza::Clock::at(itsStartTime),
    //   em microssegundos desde 2004-01-01 00:00:00 UTC.
    //
    // elapsedSimulationUs:
    //   tempo decorrido no domínio de simulação, em microssegundos.
    //
    // Produz TimestampIts em milissegundos. A conversão para ms trunca
    // a fração inferior a 1 ms. Não consulta o relógio do sistema.
    static bool BuildReferenceTime(
        std::int64_t itsStartClockUs,
        std::int64_t elapsedSimulationUs,
        std::int64_t& outputReferenceTimeItsMs,
        std::string& error);

    // Delta = instante da medição - referenceTime.
    // O instante da medição pode ser anterior ou posterior à referência.
    // O perfil R2 auditado limita o delta a [-2048, 2047] ms.
    static bool ComputeMeasurementDeltaTimeMs(
        std::int64_t referenceTimeItsMs,
        std::int64_t measurementTimeItsMs,
        std::int32_t& outputDeltaMs,
        std::string& error);

    // Idade = referenceTime - primeira observação do objeto pela estação.
    // CPM R2: a idade é limitada ao intervalo [0, 1500] ms.
    // 1500 ms é o valor de saturação para pistas mais antigas.
    // Não calcular a idade a partir da última medição: são grandezas
    // temporais distintas.
    static bool ComputeObjectAgeMs(
        std::int64_t referenceTimeItsMs,
        std::int64_t firstObservedTimeItsMs,
        std::int32_t& outputAgeMs,
        std::string& error);
};

#endif
