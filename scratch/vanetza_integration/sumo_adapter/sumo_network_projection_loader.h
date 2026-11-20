#pragma once

#include <string>

#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"

namespace ns3
{

class SumoNetworkProjectionLoader
{
public:
    // Lê o elemento <location> de uma rede SUMO e aceita apenas
    // projeções UTM com datum WGS84 e unidades métricas.
    //
    // Extrai:
    //   - zona UTM;
    //   - hemisfério (norte por omissão, sul com +south);
    //   - netOffset x/y.
    //
    // Em caso de falha, preserva output e preenche error.
    // Em caso de sucesso, atualiza output e limpa error.
    static bool LoadUtmWgs84(
        const std::string& netFile,
        SumoUtmWgs84ProjectionConfig& output,
        std::string& error);
};

} // namespace ns3
