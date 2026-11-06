#pragma once

#include <cstdint>
#include <string>

namespace vanetza_integration
{
namespace vam
{

// Identificador ITS da estação.
// A representação é uint32_t, mas não existe conversão implícita
// entre este tipo e um inteiro genérico.
struct VamStationId
{
    std::uint32_t value{};
};

// Referências opacas e semanticamente distintas.
// Nenhuma destas referências representa stationId,
// identidade criptográfica ou identidade pessoal persistente.
struct SumoPersonReference
{
    std::string value;
};

struct Ns3NodeReference
{
    std::string value;
};

struct MobilitySourceReference
{
    std::string value;
};

struct LogicalItsStationReference
{
    std::string value;
};

} // namespace vam
} // namespace vanetza_integration
