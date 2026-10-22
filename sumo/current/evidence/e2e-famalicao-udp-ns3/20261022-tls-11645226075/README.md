# E2E SUMO/TraCI → UDP → ns-3 — Vila Nova de Famalicão

- Data: 2026-10-22
- Resultado: PASS para estados de veículo e timestamps lógicos.
- Configuração: famalicao-demo.sumocfg
- Rede: famalicao.net.xml
- TLS selecionado para esta execução: 11645226075
- Binário SUMO utilizado pela bridge: /usr/bin/sumo
- Binário ns-3: build/scratch/ns3.47-v2x_sumo-default
- Código de saída da bridge: 0
- Código de saída do ns-3: 0

## Evidência observada

- Linhas SUMO -> NS3: 406.
- Datagramas de estado de veículo identificados: 80.
- Estados RX_VEHICLE_STATE: 80.
- Timestamps distintos no emissor: 80.
- Timestamps distintos no receptor: 80.
- Valores de timestamp coincidentes: 80.
- Correlação automatizada: PASS.

Os registos SUMO_TLS demonstram publicação pela bridge, mas este ensaio
não demonstra que o ns-3 tenha descodificado ou consumido essas mensagens.

## Referenciais

O timestamp UDP é o tempo lógico SUMO, em microssegundos. As coordenadas
x/y do datagrama são posições no referencial projetado da rede SUMO,
não longitude/latitude. A conversão geográfica através de TraCI foi
validada separadamente; este ensaio não transporta WGS84 no datagrama
nem demonstra integração CPM.

## Hashes dos artefactos usados

- Bridge oficial: 7d1f02cedcda4c37aaea01330d00e8fcb2f7032beb45f8323086b8cf0a3ed2e6
- Rede SUMO: 2b81c23f3f88bf543f258fe09a67c7b4977b191cb8e6863cdbaed8cbc0587dd9
- Configuração: b583c9d2c4feaea5c0a19a21347f8fbdda272fdae9a518e02665717527f71310
- Rotas: 18f2c6938ca1c8c8c017df71003cc0cdd4d90bef68b26b4a7b1fc9b7e8174ecd
- Fonte OSM: a7c21b2526fa12d3c36e279b78ac2ba08259b01fdc78cc860bbc6ca0d97f9b6e

## Ficheiros

- bridge-output.txt: registos do emissor
- ns3-output.txt: registos do receptor
- correlation-results.txt: contagens e resultado da correlação
- SHA256SUMS: manifesto dos ficheiros desta evidência

## Limitações

A execução usou uma cópia temporária da bridge e selecionou apenas o
semáforo 11645226075. Não valida a substituição dos caminhos antigos
no runtime de produção, o tratamento RX das mensagens de semáforo,
o pipeline CPM, a origem/transformação de coordenadas dentro do CPM
ou interoperabilidade externa.
