#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_COORDINATE_QUANTIZER_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_COORDINATE_QUANTIZER_H

#include <cstdint>
#include <string>

class CpmCoordinateQuantizer
{
public:
    // CartesianCoordinateLarge: unidade de 0,01 m.
    // Os extremos ASN.1 são códigos reservados para indicar fora de alcance.
    static constexpr std::int32_t kNegativeOutOfRange = -131072;
    static constexpr std::int32_t kMinNormalValue = -131071;
    static constexpr std::int32_t kMaxNormalValue = 131070;
    static constexpr std::int32_t kPositiveOutOfRange = 131071;

    static constexpr double kMinNormalMetersExclusive = -1310.72;
    static constexpr double kMaxNormalMetersInclusive = 1310.70;

    // Converte metros para o inteiro ASN.1.
    //
    // Os intervalos normais seguem a regra do CDD:
    // (n - 1) * 0,01 m < posição <= n * 0,01 m.
    //
    // Coordenadas fora do intervalo são representadas pelos códigos
    // reservados kNegativeOutOfRange / kPositiveOutOfRange.
    // Entradas não finitas são rejeitadas e preservam output.
    //
    // true significa que a conversão foi efetuada, incluindo quando
    // output contém um código reservado de coordenada fora do intervalo.
    // false significa entrada inválida; error fica preenchido.
    static bool ToAsn1Value(
        double meters,
        std::int32_t& output,
        std::string& error);
};

#endif
