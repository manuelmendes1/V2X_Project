#pragma once

#include <optional>
#include <string>

namespace ns3
{

// Neutral WGS84 coordinate pair. Latitude/longitude are decimal degrees.
struct Wgs84Position
{
    double latitudeDeg{};
    double longitudeDeg{};
};

// Compatibility alias for callers using the original SUMO transform API.
using SumoWgs84Position = Wgs84Position;

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

class SumoGeodeticTransform
{
public:
    // Valida uma coordenada WGS84 sem modificar os argumentos.
    static bool ValidateWgs84(
        const Wgs84Position& position,
        std::string& error);

    // Inverte a projeção UTM/WGS84, removendo primeiro o netOffset SUMO.
    static bool ToWgs84(
        const SumoUtmWgs84ProjectionConfig& projection,
        const SumoProjectedPosition& projected,
        Wgs84Position& output,
        std::string& error);

    // Resolve uma posição a partir do WGS84 fornecido pelo SUMO e/ou da
    // projeção UTM/WGS84 da rede. Se ambas existirem, verifica a coerência.
    // Sem uma fonte válida, falha fechado e preserva output.
    static bool ResolveWgs84(
        const std::optional<Wgs84Position>& suppliedWgs84,
        const std::optional<SumoUtmWgs84ProjectionConfig>& projection,
        const SumoProjectedPosition& projected,
        Wgs84Position& output,
        std::string& error);
};

} // namespace ns3
