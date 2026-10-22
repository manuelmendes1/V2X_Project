# Vila Nova de Famalicão — rede SUMO georreferenciada

Estado: NETWORK_GENERATED_PENDING_TRAFFIC_AND_RUNTIME_VALIDATION

## Fonte rodoviária

- Projeto: OpenStreetMap contributors.
- Licença dos dados: Open Database License (ODbL).
- Atribuição: © OpenStreetMap contributors.
- URL de licença e atribuição: https://www.openstreetmap.org/copyright
- Data de aquisição: 2026-10-22.
- Endpoint usado: https://overpass-api.de/api/interpreter
- Relação administrativa consultada: OSM relation 4032838.
- `osm_base` no ficheiro de origem: 2026-10-22T07:51:18Z.
- SHA-256 de `source/famalicao-roads.osm`:
  `a7c21b2526fa12d3c36e279b78ac2ba08259b01fdc78cc860bbc6ca0d97f9b6e`

A consulta inclui vias com a etiqueta `highway` dentro da área administrativa
derivada da relação OSM. A relação declara `source=DGT - CAOP 2024.1`.
A CAOP2025 foi inspecionada separadamente e confirmou a entidade municipal
(código DTMN 0312), mas o ficheiro OSM não foi recortado geometricamente pelo
polígono CAOP2025. Diferenças entre versões de fronteiras permanecem possíveis.

## Geração

- Conversor: `/usr/bin/netconvert`.
- Versão: SUMO 1.25.0.
- Projeção solicitada: UTM automática a partir das coordenadas geográficas.
- Comando base:
  `/usr/bin/netconvert --osm-files source/famalicao-roads.osm --output-file famalicao.net.xml --proj.utm true`
- Log de geração: `netconvert.log` (artefacto local, gerado e não versionado).

## Limites

Esta rede é uma importação de dados OSM, não um levantamento oficial nem uma
garantia de completude de todas as estradas. A presença de geometria e
metadados de projeção não valida por si só a conectividade viária, as restrições
de circulação, os semáforos, as rotas SUMO ou a compatibilidade com TraCI/ns-3.

Não usar como runtime de produção até à validação da rede, criação das rotas e
configuração de simulação, teste de runtime e auditoria dos caminhos da bridge.

O cenário legado permanece em `../legacy-local/`.

## Demo scenario: `famalicao-demo.sumocfg`

A demonstration demand scenario was generated using SUMO 1.25.0 tools
from `/usr/share/sumo/tools`, with seed 42, a 600-second period, a
10-second insertion interval, minimum trip distance 500 m and passenger
vehicle class.

Validation evidence from the temporary test:
- Requested trips: 60.
- Vehicles with computed routes: 60.
- Route success rate: 100%.
- SUMO simulation with the resulting routes: exit code 0.

Permanent demonstration artifacts:
- `famalicao-demo.trips.xml`: requested trips.
- `famalicao-demo.rou.xml`: routed vehicles.
- `famalicao-demo.sumocfg`: 600-second scenario configuration.

Run from this directory with:

    /usr/bin/sumo -c famalicao-demo.sumocfg

The generated network is a large local artifact and is excluded from Git.
The OSM source and projection-generation command are retained so the network
can be reconstructed with the compatible system converter:

    /usr/bin/netconvert --osm-files source/famalicao-roads.osm --output-file famalicao.net.xml --proj.utm true

Do not switch the production bridge to this scenario until the migration
and runtime integration have been audited separately.
