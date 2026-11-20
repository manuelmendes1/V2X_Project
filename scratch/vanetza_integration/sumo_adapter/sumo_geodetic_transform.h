#pragma once

#include <string>

namespace ns3
{

// Configuração explícita da projeção UTM/WGS84 usada pela rede SUMO.
// netOffset corresponde aos dois valores do atributo <location netOffset>.
struct SumoUtmWgs84ProjectionConfig
{
    int zone{};
    bool northernHemisphere{};
    double netOffsetXMeters{};
    double netOffsetYMeters{};
};

// Posição projetada absoluta no referencial da rede SUMO, em metros.
struct SumoProjectedPosition
{
    double xMeters{};
    double yMeters{};
};

// Posição geográfica WGS84 em graus decimais.
struct SumoWgs84Position
{
    double latitudeDeg{};
    double longitudeDeg{};
};

class SumoGeodeticTransform
{
public:
    // Converte coordenadas SUMO para WGS84 através da inversão UTM.
    //
    // Só suporta UTM/WGS84. A zona, o hemisfério e netOffset têm de ser
    // fornecidos explicitamente a partir da configuração da rede.
    //
    // Em caso de falha, devolve false, preenche error e preserva output.
    // Em caso de sucesso, atualiza output e limpa error.
    static bool ToWgs84(
        const SumoUtmWgs84ProjectionConfig& projection,
        const SumoProjectedPosition& projected,
        SumoWgs84Position& output,
        std::string& error);
};

} // namespace ns3
