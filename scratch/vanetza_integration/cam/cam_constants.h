#pragma once

// ======================================================
// CAM POSITION ENCODING
// ======================================================
// A rede SUMO atual não tem georreferenciação real
// (projParameter="!" em simple.net.xml), pelo que as posições
// vindas do SUMO/TraCI são coordenadas cartesianas locais (metros),
// não latitude/longitude WGS84 reais.
//
// Até existir georreferenciação real (fase futura), mapeamos essas
// coordenadas locais diretamente para os campos ASN.1
// referencePosition.latitude/longitude do CAM, usando uma escala fixa
// que mantém os valores dentro do intervalo válido (constrangido) do
// tipo Latitude/Longitude do ETSI ITS CDD (~±900000000 / ±1800000000),
// evitando overflow / encoding inválido.
//
// IMPORTANTE: este valor TEM de ser idêntico no encoder e no decoder.
// Está centralizado aqui exatamente para evitar a dessincronização
// que já causou bugs anteriormente.
//
// Margem atual: rede até ~500m por eixo -> 500 * 1e6 = 5e8,
// confortavelmente dentro do limite. Se o cenário crescer para
// redes maiores (>900m num eixo), este valor terá de ser revisto.
constexpr double kCamPositionScale = 1e6;
