#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_COORDINATE_TRANSFORM_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_COORDINATE_TRANSFORM_H

#include <optional>
#include <string>

#include "vanetza_integration/cpm/types/cpm_domain_types.h"

// Posição absoluta no referencial cartesiano local do cenário, em metros.
// O contrato do cenário deve definir x positivo como Este, y positivo como
// Norte e z positivo como Cima. Isto não representa latitude/longitude WGS84.
struct CpmLocalPosition
{
    double xMeters{};
    double yMeters{};
    std::optional<double> zMeters;
};

class CpmCoordinateTransform
{
public:
    // Calcula a posição do objeto relativamente à estação de referência,
    // expressa como x Este, y Norte e z Cima, em metros.
    //
    // Pressupõe que os eixos locais do cenário já estão alinhados com os
    // eixos Este/Norte/Cima. Não aplica rotação cartográfica nem projeção
    // geográfica.
    //
    // Se a transformação falhar, devolve false, preenche error e preserva
    // output. Em caso de sucesso, limpa error. zUpMeters só é fornecido
    // quando as duas posições possuem altitude válida.
    static bool ToEnuRelative(
        const CpmLocalPosition& stationPosition,
        const CpmLocalPosition& objectPosition,
        CpmCartesianPosition3D& output,
        std::string& error);
};

#endif
