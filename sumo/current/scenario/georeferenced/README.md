# Nova rede SUMO georreferenciada

Estado: SOURCE_AND_PROJECTION_VALIDATED; E2E_STATE_FEED_VALIDATED;
FULL_NETWORK_AND_TRAFFIC_AUDIT_PENDING.

A rede de demonstração de Vila Nova de Famalicão tem fonte OpenStreetMap
identificada, metadados WGS84/UTM zona 29 e um teste de projeção independente
PROJ contra o nó OSM `11645226075`. O erro observado entre a projeção calculada
e o junction SUMO foi de 0,005682 m.

A bridge SUMO/TraCI foi exercitada E2E com envio UDP de estados e coordenadas
WGS84 persistidas em ns-3. O percurso CPM de construção e codificação do
subconjunto suportado também foi exercitado. Estes resultados não certificam
a precisão topográfica da cartografia, a completude da rede viária, todos os
mapeamentos de semáforos nem a interoperabilidade de mensagens CPM.

A rede gerada `vila-nova-de-famalicao/famalicao.net.xml` é um artefacto local
grande e não é versionada. Pode ser reconstruída a partir da fonte OSM com
o conversor compatível indicado no README do cenário.

O cenário legado em `../legacy-local/` permanece separado e inalterado.
