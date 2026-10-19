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
// Domínio do cenário RiLSA de teste: coordenadas locais entre 0 e 1000 m
// por eixo. A escala sintética 8e5 mantém 1000 m em 800 000 000,
// dentro do limite ASN.1 de Latitude (aproximadamente +/-900 000 000).
//
// Esta representação destina-se ao protótipo sem georreferenciação real.
// Não converte coordenadas cartesianas locais em latitude/longitude WGS84.
// Quando existir georreferenciação real, deve ser usada uma transformação
// geográfica adequada.
//
// O encoder e o decoder têm de usar o mesmo fator.
constexpr double kCamPositionScale = 8e5;
