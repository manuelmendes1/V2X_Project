#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_GEODETIC_TRANSFORM_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_GEODETIC_TRANSFORM_H

#include <string>

#include "vanetza_integration/cpm/types/cpm_domain_types.h"

// Posição geodésica WGS84.
// latitudeDeg e longitudeDeg são expressas em graus decimais.
// ellipsoidHeightMeters é a altura elipsoidal WGS84, em metros;
// não deve ser confundida com altitude ortométrica.
struct CpmGeodeticPosition
{
    double latitudeDeg{};
    double longitudeDeg{};
    double ellipsoidHeightMeters{};
};

// Posição geodésica horizontal WGS84, sem presumir altitude.
struct CpmGeodeticHorizontalPosition
{
    double latitudeDeg{};
    double longitudeDeg{};
};

class CpmGeodeticTransform
{
public:
    // Produz componentes horizontais Este/Norte a partir da geodésia
    // WGS84. Não exige nem inventa alturas elipsoidais; zUp fica ausente.
    static bool ToEnuHorizontalRelative(
        const CpmGeodeticHorizontalPosition& station,
        const CpmGeodeticHorizontalPosition& object,
        CpmCartesianPosition3D& output,
        std::string& error);

    // Converte a posição geodésica do objeto para ENU relativamente
    // à posição geodésica da estação.
    //
    // A origem ENU é a posição da estação, incluindo a sua altura
    // elipsoidal. O resultado usa metros:
    //   xEastMeters  > 0: Este
    //   yNorthMeters > 0: Norte
    //   zUpMeters    > 0: Cima
    //
    // Utiliza o elipsoide WGS84 através da GeographicLib.
    //
    // Se os argumentos forem inválidos ou a transformação falhar,
    // devolve false, define error e preserva output.
    // Em caso de sucesso, atualiza output e limpa error.
    static bool ToEnuRelative(
        const CpmGeodeticPosition& station,
        const CpmGeodeticPosition& object,
        CpmCartesianPosition3D& output,
        std::string& error);
};

#endif
