---
title: "Etapa 10.4 — Fonte de perceção, coordenadas e tempo"
status: "COMPLETE"
tags: [V2X, CPM, perception, coordinates, time, validation, testing]
---

# Etapa 10.4 — Fonte de perceção, coordenadas e tempo

**Estado:** COMPLETE — critérios de saída da Etapa 10.4 satisfeitos para as transformações espaciais/temporais e a fonte de perceção implementadas; a precisão topográfica absoluta, a sincronização com relógios físicos, a transmissão CPM e a interoperabilidade não são declaradas como provadas.
**Data da evidência inicial:** 2026-10-21
**Última revalidação WGS84 E2E:** 2026-10-22
**Perfil de referência:** ETSI TS 103 324 V2.1.1, Release 2
**CDD instalado auditado:** `TS102894-2v241-CDD.asn`
**Branch:** `feature/cpm-etapa10-4-perception-coordinate-time`
**Commit-base:** `70aeaf6` — merge da Etapa 10.3
**Estado Git:** artefactos ainda não commitados

## 1. Objetivo e âmbito

A fase introduz uma transformação cartesiana relativa, um quantizador explícito para coordenadas CPM R2, funções de referência e delta temporal e uma fonte sintética de perceção com alcance horizontal configurável.

A fase começou pela validação isolada das primitivas espaciais, temporais e da fonte sintética. A revalidação E2E da secção 13 acrescenta uma fonte de perceção alimentada por estados de veículos persistidos em ns-3, recebidos pela bridge SUMO/TraCI via UDP, e a codificação UPER do subconjunto suportado. A fonte sintética continua a ser um componente separado; a fonte ligada ao controlador é CpmVehiclePerceptionSource.

O estado COMPLETE refere-se aos critérios de saída da Etapa 10.4 documentados na secção 7 e à evidência final da secção 14. Não significa certificação topográfica independente, sincronização com um relógio físico, cobertura integral do perfil CPM R2, transmissão CPM ou interoperabilidade externa.

## 2. Artefactos

### Transformação espacial

- `scratch/vanetza_integration/cpm/perception/cpm_coordinate_transform.h`
- `scratch/vanetza_integration/cpm/perception/cpm_coordinate_transform.cc`
- `scratch/test/Green/CPM/test_cpm_coordinate_transform.cc`
- `scratch/test/Green/CPM/test_cpm_coordinate_transform.py`

### Quantização de coordenadas

- `scratch/vanetza_integration/cpm/perception/cpm_coordinate_quantizer.h`
- `scratch/vanetza_integration/cpm/perception/cpm_coordinate_quantizer.cc`
- `scratch/test/Green/CPM/test_cpm_coordinate_quantizer.cc`
- `scratch/test/Green/CPM/test_cpm_coordinate_quantizer.py`

### Transformação temporal

- `scratch/vanetza_integration/cpm/perception/cpm_temporal_transform.h`
- `scratch/vanetza_integration/cpm/perception/cpm_temporal_transform.cc`
- `scratch/test/Green/CPM/test_cpm_temporal_transform.cc`
- `scratch/test/Green/CPM/test_cpm_temporal_transform.py`

### Fonte sintética

- `scratch/vanetza_integration/cpm/perception/cpm_synthetic_perception_source.h`
- `scratch/vanetza_integration/cpm/perception/cpm_synthetic_perception_source.cc`
- `scratch/test/Green/CPM/test_cpm_synthetic_perception_source.cc`
- `scratch/test/Green/CPM/test_cpm_synthetic_perception_source.py`

## 3. Contrato espacial

### 3.1 Transformação relativa

`CpmCoordinateTransform::ToEnuRelative` calcula:

- `xEastMeters = object.xMeters - station.xMeters`
- `yNorthMeters = object.yMeters - station.yMeters`
- `zUpMeters = object.zMeters - station.zMeters`, apenas quando ambas as altitudes estão disponíveis.

As posições locais são expressas em metros. As entradas e os resultados são verificados quanto a valores não finitos. Em caso de falha, a saída mantém o valor anterior.

A fonte sintética declara a estação como RSU (`CpmRoadSideUnitStationData`), coerente com o contrato ENU adotado para os objetos deste cenário. A assertion correspondente passou no teste da fonte sintética. Esta declaração não demonstra que as coordenadas locais de uma rede SUMO real estejam alinhadas com Este/Norte/Cima.

**Limite do contrato:** o algoritmo pressupõe que os eixos da fonte já estão alinhados com Este, Norte e Cima. Não efetua rotação cartográfica, projeção nem conversão de longitude/latitude. Os testes cardeais comprovam a convenção aritmética, não o alinhamento geográfico da rede SUMO.

A rede consultada usa `projParameter="!"`; essa configuração não fornece uma projeção geográfica utilizável para demonstrar uma conversão real para WGS84. Quando não existe posição geográfica válida, a fonte deixa os campos geográficos de referência indisponíveis em vez de fabricar coordenadas.

### 3.2 Quantização para `CartesianCoordinateLarge`

A API `CpmCoordinateQuantizer::ToAsn1Value` converte metros para o inteiro escalado do CDD R2, cuja unidade é 0,01 m.

A regra aplicada é equivalente ao intervalo:

`(n - 1) × 0,01 m < posição ≤ n × 0,01 m`

para cada código normal `n`, tratando os limites e os valores especiais separadamente.

| Propriedade | Valor |
|---|---:|
| Código ASN.1 negativo reservado para fora do intervalo | `-131072` |
| Menor código normal | `-131071` |
| Maior código normal | `131070` |
| Código ASN.1 positivo reservado para fora do intervalo | `131071` |
| Limite inferior normal | `-1310,72 m`, exclusivo |
| Limite superior normal | `+1310,70 m`, inclusivo |

Uma coordenada finita fora do intervalo normal devolve o código reservado correspondente. A função devolve `false` para entradas não finitas, preenche o erro e preserva a saída anterior. A confiança da coordenada é um campo separado e não é quantizada por esta API.

A conversão é explícita e não é aplicada implicitamente pela transformação espacial. CpmUperEncoder utiliza o quantizador para os campos de coordenadas horizontais suportados. Isto não demonstra cobertura integral dos campos e opcionais CPM R2.

### 3.3 Evidência espacial

O teste da transformação passou oito verificações: Norte, Sul, Este, Oeste, deslocamento 3D, altitude relativa indisponível, coordenada NaN e altitude infinita.

O teste do quantizador passou 18 verificações, incluindo valores inferiores a um centímetro, fronteiras positivas e negativas, limites normais, códigos reservados, valores adjacentes aos limites e rejeição de NaN e infinito.

## 4. Fonte sintética e alcance

`CpmSyntheticPerceptionSource::Build` constrói uma entrada CPM determinística a partir de observações fornecidas explicitamente. Cada observação contém identificador, posição, instante de medição e instante da primeira observação.

`perceptionRangeMeters` define um alcance horizontal em metros, com predefinição de 50 m. O valor tem de ser finito e não negativo. A distância usada no filtro é:

`sqrt(deltaX² + deltaY²)`

Os objetos à distância exatamente igual ao limite são incluídos; os objetos para além do limite não são incluídos. O componente vertical não afeta a distância de inclusão. O alcance zero permite incluir objetos coincidentes horizontalmente com a estação.

A implementação valida identificadores não nulos, identificadores duplicados, posições e dados temporais. A construção é atómica: em caso de erro, a saída fornecida pelo chamador permanece inalterada.

`knownObjectCount` representa o total conhecido na LDM, e não é substituído pelo número de objetos incluídos. O construtor rejeita o cenário se, depois da seleção espacial, a lista incluída contiver mais objetos do que o total declarado.

O cenário é idealizado. A fonte não modela ruído, oclusões, campo de visão, erro de medição ou uma política física de sensor. A política atual consiste apenas no alcance horizontal configurado.

### Evidência da fonte

Foram aprovadas dez verificações, incluindo cenário válido, identificadores duplicados e nulos, tempos inválidos, coerência de contagem, alcance exato, exclusão fora do alcance, alcance zero e rejeição de valores de alcance inválidos.

## 5. Contrato temporal

### 5.1 Referência CPM

`BuildReferenceTime` recebe:

- `itsStartClockUs`: instante inicial do relógio ITS, em microssegundos desde 2004-01-01 00:00:00 UTC;
- `elapsedSimulationUs`: tempo decorrido, em microssegundos, no domínio de simulação declarado pelo chamador.

A função soma os valores, após validação de sinais e overflow, e converte para milissegundos por truncagem da fração inferior a 1 ms. O resultado deve estar no intervalo `TimestampIts` usado pelo perfil R2.

Esta operação presume explicitamente que o tempo decorrido fornecido pode ser somado ao instante inicial ITS. Não sincroniza relógios, não mede deriva e não converte automaticamente tempos SUMO, ns-3 ou Unix para o domínio ITS.

### 5.2 Delta da medição

`ComputeMeasurementDeltaTimeMs` calcula:

`measurementTimeItsMs - referenceTimeItsMs`

O intervalo permitido é `[-2048, 2047]` ms. Um delta negativo indica um instante anterior à referência; um delta positivo indica um instante posterior, que pode ser relevante para estados preditos quando a semântica e o processamento da fonte o justificarem.

O instante de medição deve ser o associado ao estado comunicado, após considerar o processamento do sensor ou da fusão aplicável. Não deve ser substituído implicitamente pelo instante de receção da mensagem.

### 5.3 Idade do objeto

`ComputeObjectAgeMs` calcula:

`referenceTimeItsMs - firstObservedTimeItsMs`

A implementação calcula a diferença entre a referência e a primeira observação e satura a idade em `[0, 1500]` ms. O valor de 1500 ms representa a saturação para pistas mais antigas. Uma primeira observação posterior à referência é rejeitada. A regra usa a primeira observação pela estação como origem temporal; pressupõe que o cenário sintético representa corretamente esse histórico.

A referência CPM, o instante da medição, a primeira observação e o instante de receção são grandezas distintas.

### 5.4 Evidência temporal

O teste temporal passou 26 verificações, incluindo:

- instantes conhecidos e época;
- limite máximo representável;
- truncagem aos 999 µs finais e rejeição quando a conversão ultrapassa o limite;
- valores negativos, fora do intervalo e overflow;
- deltas positivos, negativos, limites válidos e valores inválidos;
- deltas sobre todo o intervalo temporal rejeitados;
- idade zero, idade máxima, primeira observação futura e idade excessiva.

Estes testes demonstram a aritmética isolada. Não demonstram sincronização de relógios, ingestão temporal real ou processamento temporal no caminho de produção.

## 6. Evidência de regressão e build

Resultados executados no workspace em 2026-10-21:

| Verificação | Resultado |
|---|---|
| Transformação cartesiana isolada | PASS — 8 verificações |
| Transformação geodésica WGS84 → ENU isolada | PASS — 11 verificações; GeographicLib compilada e ligada |
| Quantizador de coordenadas isolado | PASS — 18 verificações |
| Contrato temporal isolado, após os novos testes de fronteira | PASS — 26 verificações |
| Fonte sintética isolada | PASS — 12 verificações |
| Ciclo de vida temporal sintético | PASS — progressão da idade em observação contínua e estado predito com delta positivo testados |
| Origem da fonte sintética | PASS — RSU (`CpmRoadSideUnitStationData`), assertion aprovada |
| Última suite Green completa executada após as alterações RSU/ENU, geodésica e do ciclo temporal | PASS — 26 testes, 0 falhas, 0 erros |
| Última suite Green + Red completa executada | PASS — 48 testes, 0 falhas, 0 erros |
| Testes do analyzer | PASS histórico — 43 testes sobre corpus recuperado |
| Build limpo de `ns3.47-v2x_sumo-default` | PASS — exit code 0 |

A suite Green e a suite completa Green + Red foram reexecutadas após a integração do histórico persistente. A suite Green passou com 26 testes e a suite completa passou com 48 testes, sem falhas nem erros. A fonte sintética passou 12 verificações e o teste de `CpmTrackHistory`, incluindo a integração por `BuildWithHistory`, passou 14 verificações. A transformação geodésica WGS84 → ENU passou 11 verificações. O build limpo de `ns3.47-v2x_sumo-default` também passou. Os resultados numéricos anteriores descrevem as suites e execuções realizadas à data indicada e não são substituídos pelos seus resultados posteriores. A evidência posterior da secção 12 valida o transporte e a persistência WGS84; a secção 13 acrescenta a construção e codificação UPER a partir dos estados SUMO recebidos por UDP, no alvo configurado ns3.47-v2x_sumo-default. O mapeamento temporal usado na execução parte do timestamp lógico SUMO e do instante inicial ITS configurado; a evidência não demonstra sincronização de relógios físicos nem validação independente de toda a semântica temporal.

**Limitação do analyzer:** os 43 testes foram executados sobre `test-results/cpm_etapa10_0_baseline_20261019T155725Z/recovered_analyzer_tests/scratch/v2x_analyzer/tests`. A pasta `scratch/v2x_analyzer/tests` não está presente no checkout atual. Este resultado é regressão contra o corpus recuperado, não prova de uma suite versionada nesse diretório.

## 7. Critérios de saída

- [x] Fonte sintética determinística definida e testada.
- [x] Origem da fonte declarada como RSU, com contrato ENU sintético explícito e assertion testada.
- [x] Alcance horizontal configurável, finito e não negativo.
- [x] Inclusão na fronteira e exclusão fora do alcance testadas.
- [x] Semântica de `knownObjectCount` preservada separadamente da lista incluída.
- [x] Transformação cartesiana relativa independente do codec testada.
- [x] Primitiva geodésica WGS84 → ENU baseada em GeographicLib, compilada e testada independentemente — 13 verificações, incluindo duas referências PROJ para o caminho horizontal de produção.
- [x] Convenção de sinal Norte/Sul/Este/Oeste testada sob o pressuposto X=Este/Y=Norte.
- [x] Quantização de coordenadas definida para `CartesianCoordinateLarge` e protegida por testes de fronteira.
- [x] Valores não finitos detetados nas posições e operações abrangidas.
- [x] Referência temporal, delta de medição e idade implementados e testados.
- [x] Semântica temporal sintética testada: a idade evolui com a referência e o estado predito usa delta positivo sem reiniciar a idade.
- [x] Limites temporais, truncagem e grandes diferenças testados.
- [x] Limitações da georreferenciação absoluta, da semântica temporal e da integração via bridge UDP, distinta de uma fonte que consuma diretamente a API TraCI, documentadas.
- [x] Validar a proveniência geográfica da rede SUMO no ponto OSM `11645226075`, com projeção UTM zona 29/WGS84 e `netOffset`, usando PROJ independente. Erro observado: 0,005682 m; isto valida a coerência da projeção da rede, não a exatidão topográfica absoluta.
- [x] Validar a proveniência da referência temporal configurada e a aritmética dos instantes: `--itsStartTime` é convertido por `vanetza::Clock::at`; um instante UTC conhecido é conferido de forma independente e a progressão SUMO→TimestampIts é demonstrada E2E. Não se infere sincronização com relógios físicos ou sensores reais.
- [x] Documentar e validar a relação operacional entre timestamp lógico SUMO, fronteira de processamento ns-3 e `TimestampIts`: `SUMO_STEP_END` transporta o instante lógico para o controlador, que soma esse instante à origem ITS explícita. O relógio do scheduler ns-3 não entra nessa aritmética; a relação comprovada é causal, não uma sincronização numérica entre relógios físicos.
- [x] Executar as suites finais CPM, CAM e SPATEM após as alterações aos testes — PASS: CPM Green 11/11, CPM Red 6/6, CAM Green 1/1, CAM Red 5/5, SPATEM Green 1/1 e SPATEM Red 17/17; total 41 testes, sem falhas nem erros.
- [x] Validar o alvo `ns3.47-v2x_sumo-default` após as alterações — PASS, código de saída 0 (`ninja: no work to do`); o build limpo continua documentado nos resultados históricos da secção 6.

## 8. Exclusões explícitas

Esta fase ainda não demonstra:

- validação da exatidão geodésica absoluta contra um ponto de controlo independente;
- sincronização da referência ITS com relógios físicos externos, medição de offset/deriva e timestamps de um sensor real;
- um sensor físico ou um modelo de perceção com ruído e oclusões;
- cobertura integral de todos os campos e opcionais do perfil CPM R2;
- transmissão CPM por BTP/GeoNetworking;
- integração de CPM em PacketContext, security, replay, delivery ou analyzer;
- interoperabilidade externa ou conformidade ETSI global.

A integração atual recebe estados pela bridge SUMO/TraCI através de UDP; não equivale a uma fonte CPM que consome diretamente a API TraCI nem a um sensor físico.

## 9. Decisão histórica (antes do fecho final)

> Nota: a decisão PARTIAL desta secção reflete a evidência disponível antes dos ensaios independentes e da regressão final da secção 14. A decisão atual encontra-se na secção 14.

**Decisão: PARTIAL — não aceitar ainda a saída da Etapa 10.4.**

Aceitam-se como evidência parcial as primitivas espaciais e temporais, a quantização explícita, a fonte sintética com alcance configurável e a integração E2E documentada na secção 13: estados SUMO recebidos por UDP, construção CPM e codificação UPER do subconjunto suportado, com controlo da fronteira temporal por passo.

A falta de validação geodésica absoluta independente, a semântica temporal SUMO/ns-3/ITS ainda não validada de forma independente, a cobertura parcial do perfil CPM R2 e a ausência de transmissão/interoperabilidade impedem declarar a fase concluída. O estado global do plano `ETAPA_10_CPM_PLANO.md` permanece inalterado.

**Próximo trabalho:** validar as coordenadas contra um ponto geodésico independente, concluir a auditoria semântica dos relógios, ampliar a cobertura do perfil CPM R2 e, numa etapa separada, validar transmissão e receção CPM.
## Revalidação após integração do histórico persistente — 2026-10-21

### Resultados observados

- Teste focado de `CpmSyntheticPerceptionSource`: PASS.
- Teste do histórico persistente `CpmTrackHistory` e integração com a fonte sintética: PASS, 14 verificações.
- Suite Green: 26 testes, 0 falhas e 0 erros.
- Suite completa Green + Red: 48 testes, 0 falhas e 0 erros.
- Build limpo do alvo configurado `ns3.47-v2x_sumo-default`: PASS.
- Plano global `ETAPA_10_CPM_PLANO.md`: inalterado.
- Repositório sem novos commits; as alterações da Etapa 10.4 permanecem por rever.

### Interpretação e limites da evidência

O histórico mantém o instante da primeira observação por identificador, atualiza a observação mais recente e rejeita tempos regressivos sem alterar o estado. A integração testada através de `BuildWithHistory` conserva a separação entre o instante de medição e a idade do objeto, incluindo o caso de estado previsto.

Estas provas usam cenários sintéticos. O build bem-sucedido valida o alvo atualmente configurado, mas não demonstra que os novos módulos CPM estão incluídos ou ligados nesse alvo de produção.

Continuam por demonstrar a origem geográfica real e o alinhamento SUMO/rede, a integração de observações CPM reais via SUMO/TraCI, o mapeamento entre os relógios de simulação SUMO/ns-3 e a referência temporal ITS e a integração dos módulos CPM no pipeline/alvo de produção. A passagem do timestamp lógico SUMO até ao estado de veículo em ns-3 foi validada separadamente.

### Decisão

**PARTIAL — não aceitar ainda a saída da Etapa 10.4.** Os testes unitários e sintéticos do histórico persistente passam, mas os critérios de integração real e de proveniência das coordenadas e do tempo permanecem por validar.

## 10. Revalidação E2E do timestamp lógico SUMO/TraCI — 2026-10-21

### Execução

Foi realizada uma execução controlada com o cenário `simple.sumocfg`,
o emissor SUMO/TraCI e o alvo `ns3.47-v2x_sumo-default`.

Foi utilizada uma cópia temporária da bridge. Nessa cópia, a função
`should_continue_simulation` foi adaptada exclusivamente para impor um limite
de cinco segundos de tempo lógico SUMO. Esta alteração foi necessária para
terminar controladamente o cenário sem semáforos. A serialização do timestamp
foi mantida, e a bridge original não foi alterada por este teste.

Parâmetros relevantes:

- Limite de tempo lógico SUMO: 5 segundos.
- Passo SUMO: 0,1 segundos.
- ns-3: `--simDuration=12 --enableCam=0 --enableDenm=0`.
- Origem temporal: `traci.simulation.getTime()`.
- Conversão: segundos de simulação para microssegundos, serializados no sétimo
  campo do datagrama de estado do veículo.
- Emissor: `/tmp/cpm-sumo-ts-controlled.KICzZP/bridge.log`.
- Receptor: `/tmp/cpm-sumo-ts-controlled.KICzZP/ns3.log`.

Os logs acima encontram-se numa diretoria temporária local, fora do repositório.
Devem ser preservados separadamente caso seja necessário manter a evidência
bruta depois de uma limpeza de `/tmp`.

### Resultados observados

- Pré-validação: `PASS`.
- Bridge de teste: código de saída `0`.
- Processo ns-3: código de saída `0`.
- Registos `SUMO -> NS3 |`: 108. Esta contagem inclui eventos DENM e não
  representa exclusivamente datagramas de estado de veículo.
- Registos `RX_VEHICLE_STATE`: 104.
- Valores distintos de timestamp detetados no emissor: 50.
- Valores distintos de timestamp detetados no receptor: 50.
- Valores distintos coincidentes entre os dois conjuntos: 50.
- Comparação automatizada: `SUMO_TO_NS3_TIMESTAMP_E2E=PASS`.

Os valores coincidentes incluem `100000`, `200000`, `300000` e os restantes
instantes observados durante a execução. A comparação confirmou a igualdade
dos conjuntos de 50 valores distintos. Não estabeleceu correspondência
individual de todos os datagramas por identidade de veículo.

### Interpretação e limites

Esta execução demonstra que o timestamp lógico SUMO, expresso em
microssegundos, foi serializado pela bridge e chegou ao estado de veículo
processado pelo caminho UDP em ns-3.

O timestamp representa tempo lógico da simulação SUMO. Não é tempo de relógio
de parede, não é `Simulator::Now()` e não representa, por si só, um instante
temporal ITS absoluto. O teste não demonstra sincronização de relógios,
conversão para a referência temporal exigida pelo CPM, utilização do campo
pelo gerador CPM, ingestão de observações CPM reais, nem interoperabilidade.

A terminação normal observada refere-se à cópia temporária da bridge. Não
valida a terminação da bridge original em cenários sem semáforos.

### Decisão

**PARTIAL — aceitar a passagem do timestamp lógico SUMO/TraCI até ao estado
de veículo em ns-3 como evidência parcial, sem fechar a Etapa 10.4.**

Permanecem por validar a proveniência e o alinhamento geográfico reais,
o mapeamento temporal SUMO/ns-3/ITS, a integração de observações reais na
fonte de perceção CPM e a validação dos módulos CPM no alvo de produção.

## 11. Auditoria da proveniência geográfica dos cenários SUMO — 2026-10-21

### Âmbito da inspeção

Foi inspecionado o inventário de configurações SUMO e redes disponível em
`/home/mmendes/final/v2x_sumo/scenario`, incluindo ficheiros de configuração
`.sumocfg` e redes `.net.xml`, com pesquisa de metadados de projeção e
referências explícitas a WGS84, EPSG, longitude e latitude.

### Resultados observados

As quatro redes `.net.xml` enumeradas no inventário apresentam
`projParameter="!"` e `netOffset="0.00,0.00"`:

| Ficheiro | `convBoundary` / `origBoundary` |
|---|---|
| `scenario/simple.net.xml` | `0.00,0.00,500.00,500.00` |
| `scenario/certos/simple.net.xml` | `0.00,0.00,200.00,200.00` |
| `scenario/sumo-net/mini.net.xml` | `0.00,0.00,200.00,200.00` |
| `scenario/certos/sumo-net/mini.net.xml` | `0.00,0.00,200.00,200.00` |

As configurações `simple.sumocfg`, `simple2.sumocfg` e
`p4_high_density.sumocfg` referenciam `simple.net.xml`. A configuração
`certos/simple.sumocfg` referencia `certos/simple.net.xml`.

Foi também inspecionado `integration/coisas/positions_ns3.json`. O JSON é
válido, mas contém apenas `id`, `x` e `y`; o exemplo observado é `veh1`,
`x=1.6` e `y=21.792339856031465`. Não existem campos de latitude,
longitude, CRS, projeção ou referencial. O ficheiro não documenta as unidades
nem a relação entre os eixos locais e Este/Norte.

No inventário e na pesquisa apresentados não foi encontrada uma origem
geográfica explícita nem uma projeção cartográfica que permita converter
as coordenadas locais destas redes ou do ficheiro JSON para WGS84.

### Interpretação e limites

Os metadados examinados são insuficientes para estabelecer uma transformação
geográfica real. `netOffset=(0,0)` não é uma âncora WGS84 e os limites das
redes descrevem apenas a extensão das coordenadas locais. Não se devem
inventar longitude, latitude, rotação ou orientação ENU para preencher esta
lacuna.

Esta conclusão limita-se aos ficheiros e à pesquisa efetivamente
inspecionados; não prova que não possa existir uma fonte geográfica externa
a este diretório.

### Decisão e trabalho necessário

**Critério de georreferenciação: NÃO DEMONSTRADO.**

Para o desbloquear, será necessário obter uma fonte geográfica real e
documentada — por exemplo, dados cartográficos de origem com sistema de
coordenadas conhecido — e regenerar ou configurar a rede SUMO com uma
projeção apropriada. Depois, será necessário validar a transformação e a
orientação dos eixos com pontos de controlo conhecidos.

O estado da Etapa 10.4 permanece `PARTIAL`. A primitiva WGS84 → ENU
já testada isoladamente não substitui esta evidência.


## 12. Revalidação E2E WGS84 SUMO/TraCI → ns-3 — 2026-10-22

> **Atualização da evidência:** esta secção documenta resultados obtidos
> após as auditorias anteriores. As conclusões anteriores sobre a ausência
> de origem WGS84 real são históricas e ficam supersedidas, para o transporte
> geográfico SUMO → ns-3, pelos resultados desta secção. A decisão global
> permanece PARTIAL pelas limitações indicadas em 12.6.

### 12.1. Configuração da execução

- Cenário: `sumo/current/scenario/georeferenced/vila-nova-de-famalicao/famalicao-demo.sumocfg`.
- Bridge: `sumo/current/integration/v2x_bridge_16.py`.
- SUMO/TraCI: SUMO 1.25.0.
- Binário ns-3: `build/scratch/ns3.47-v2x_sumo-default`.
- Transporte: UDP, porta 7400.
- TLS selecionado explicitamente: `11645226075`.
- Duração da simulação ns-3: 40 s; duração do bridge SUMO: 15 s.
- Auditoria do estado persistente ativada por `V2X_GEO_AUDIT=1`.
- Log de progresso por passo do SUMO desativado para evitar concatenação
  de mensagens de progresso com os registos de estado do bridge.

### 12.2. Contrato do datagrama e caminho de dados

O formato geográfico utiliza nove campos:

`vehId,x,y,speed,heading,accel,simulationTimeUs,longitudeDeg,latitudeDeg`

`x/y` mantêm-se nas coordenadas projetadas da rede SUMO. O bridge utiliza
`traci.simulation.convertGeo(x, y)` para obter longitude e latitude WGS84,
nessa ordem. `simulationTimeUs` continua a ser o tempo lógico SUMO em
microssegundos, não um timestamp absoluto ITS nem tempo de relógio de parede.

O parser preserva os formatos legados de seis campos e o formato de sete
campos com timestamp. Rejeita oito campos, timestamps inválidos, valores
não finitos e coordenadas fora dos limites geográficos. Se a conversão
falhar, o bridge envia sete campos sem inventar coordenadas WGS84.

As coordenadas geográficas são mantidas separadamente em
`VehicleMobilityContext::geodeticPosition`, como valor opcional
`VehicleGeodeticPosition`. `SumoParser::Parse()` atribui longitude e latitude
quando ambas estão disponíveis. `VehicleStateUpdater::Apply()` copia o
contexto de mobilidade para o estado persistente. O diagnóstico
`VEHICLE_STATE_GEODETIC_APPLIED` é emitido depois da atualização.

### 12.3. Evidência arquivada

Diretório da evidência:

`sumo/current/evidence/e2e-famalicao-wgs84-state/20261022T091547Z/`

Inclui os logs brutos do bridge e do ns-3, resultados de correlação, testes
focados, resultado do build, README e manifesto `SHA256SUMS`.

A verificação do manifesto passou para todos os artefactos listados.
A correlação foi recalculada diretamente a partir dos logs arquivados.

### 12.4. Resultados E2E observados

| Verificação | Resultado |
|---|---:|
| Linhas de estado de nove campos no bridge | 200 |
| Chaves únicas `(vehId, simulationTimeUs)` enviadas | 200 |
| Registos de auditoria WGS84 no ns-3 | 200 |
| Chaves únicas no estado persistente | 200 |
| Pares correlacionados | 200/200 |
| Amostras enviadas sem correspondência | 0 |
| Chaves RX inesperadas | 0 |
| Registos malformados ou duplicados | 0 |
| Estados com WGS84 indisponível | 0 |
| Divergências longitude/latitude | 0 |
| Código de saída do bridge | 0 |
| Código de saída do ns-3 | 0 |
| Teste geográfico do bridge | PASS — 3 testes |
| Regressão TLS do bridge | PASS — 12 testes |
| Contrato C++ do datagrama | PASS — 17 verificações |
| Wiring do estado geográfico | PASS — 5 verificações |
| Build do alvo `ns3.47-v2x_sumo-default` | PASS — exit code 0 |

Resultado registado: `SUMO_WGS84_E2E_FULL_COVERAGE=PASS`.

Esta execução demonstra que cada uma das 200 amostras geográficas de estado
enviadas foi correlacionada pela chave veículo + timestamp SUMO com o estado
persistente observado no ns-3, sem divergências nos valores WGS84 registados.

### 12.5. Verificação de round-trip TraCI

Num teste separado com SUMO real, o veículo `0`, ao tempo lógico
`1 000 000 µs`, apresentou aproximadamente:

- Coordenadas projetadas: `x=20489.0431 m`, `y=5179.7497 m`.
- Longitude: `-8.4330521°`.
- Latitude: `41.3851057°`.
- Erro de round-trip de conversão: `0,0068 m`.

O erro ficou abaixo do limite de teste de 0,5 m. Isto demonstra consistência
interna da conversão TraCI para o ponto testado. Não demonstra, por si só,
a exatidão absoluta da rede geográfica contra um marco independente.

### 12.6. Limitações e decisão

**Decisão: PARTIAL — não fechar ainda a Etapa 10.4.**

Ficam validados o transporte das coordenadas WGS84 e a sua persistência
no estado do veículo ns-3, no cenário e na execução documentados.

Continuam por demonstrar:

- validação da georreferenciação absoluta contra ponto geodésico independente;
- validação semântica independente do mapeamento temporal SUMO/ns-3/ITS e da
  proveniência da referência absoluta;
- cobertura integral de todos os campos e opcionais do perfil CPM R2;
- transmissão por BTP/GeoNetworking, receção remota, interoperabilidade externa
  ou conformidade ETSI global.

Dois testes DENM legados executados separadamente não arrancaram porque
importam `v2x_bridge_15`, módulo indisponível com esse nome na árvore atual.
Não são considerados aprovados por esta evidência E2E nem alteram os
resultados focados enumerados em 12.4; a manutenção dessa harness permanece
separada.

O plano global `scratch/docs/CPM/ETAPA_10_CPM_PLANO.md` permanece inalterado.

## 13. Revalidação E2E da fonte CPM e da fronteira temporal SUMO — 2026-10-22

> Esta secção acrescenta evidência posterior às secções 6, 8 e 12. As
> pendências históricas relativas à ligação dos estados SUMO à fonte de
> perceção CPM e à construção/codificação UPER ficam reavaliadas à luz dos
> ensaios desta secção. As conclusões não demonstram transmissão CPM nem
> conformidade global ETSI.

### 13.1. Fronteira temporal por passo SUMO

A bridge envia o marcador `SUMO_STEP_END,<timestamp_us>` depois dos estados
dos veículos e das atualizações TLS correspondentes ao passo. O
`SumoAdapter::Poll()` reconhece o marcador, valida o timestamp e invoca o
callback de conclusão do passo. O `SimulationController` chama então
`RunCpmPerception(referenceTimeUs)` com o instante explicitamente fornecido
pela bridge.

Esta fronteira impede que a fonte CPM observe apenas o último estado de um
lote de datagramas recebidos no mesmo ciclo de polling. Mantém-se a guarda
que impede codificar novamente um timestamp igual ou anterior ao último
timestamp codificado com sucesso.

### 13.2. Execução E2E SUMO → estado ns-3 → CPM UPER

Configuração relevante:

- Cenário: `sumo/current/scenario/georeferenced/vila-nova-de-famalicao/famalicao-demo.sumocfg`.
- Bridge: `sumo/current/integration/v2x_bridge_16.py`.
- Passo SUMO: 0,1 s; duração lógica SUMO: 15 s.
- TLS: `11645226075`.
- ns-3: `--simDuration=30`, CPM ativo e CAM/DENM/SPATEM desativados.
- RSU CPM: stationId `9001`, latitude `41.42528995`, longitude `-8.43407620`.
- Alcance de perceção: 7000 m.
- Diretório dos logs arquivados: `sumo/current/evidence/e2e-cpm-perception/20261022T133131Z/cpm-step-boundary/`.

Resultados observados:

| Verificação | Resultado |
|---|---:|
| Datagramas de veículos enviados pela bridge | 200 |
| Estados geodésicos aplicados em ns-3 | 200 |
| Timestamps lógicos SUMO distintos aplicados | 150 |
| CPM com construção aceite | 150 |
| CPM codificados por UPER com sucesso | 150 |
| Construções rejeitadas | 0 |
| Codificações rejeitadas | 0 |
| Timestamps CPM distintos | 150 |
| Registos de codificação duplicados | 0 |
| Timestamps aplicados sem codificação CPM | 0 |
| Timestamps com intervalo diferente de 100 ms | 0 |
| Codificações no timestamp final `15000000 µs` | 1 |
| Código de saída ns-3 | 0 |
| Código de saída da bridge | 0 |

Os timestamps anteriormente em falta, `5000000 µs` e `13300000 µs`, foram codificados uma vez cada. A sequência CPM cobriu os 150 instantes lógicos do cenário, desde `100000 µs` até `15000000 µs`, em passos de 100 ms.

Resultado observado: todos os critérios temporais incluídos neste ensaio passaram. Os logs brutos estão arquivados em `sumo/current/evidence/e2e-cpm-perception/20261022T133131Z/cpm-step-boundary/`; o manifesto `SHA256SUMS` da raiz do pacote permite verificar a integridade dos ficheiros preservados.

### 13.3. Ensaio de fronteira com ingestão DENM

Foi realizado um ensaio UDP controlado para verificar que o novo marcador de passo não interfere com a ingestão DENM.

- Diretório da evidência arquivada: `sumo/current/evidence/e2e-cpm-perception/20261022T133131Z/denm-boundary-ingest/`.
- Estados de veículo enviados: 20.
- Marcadores `SUMO_STEP_END` enviados: 10.
- Evento DENM enviado: 1.
- Estados geodésicos aplicados: 20.
- Registos de ingestão do evento `denm_boundary_test_001`: 1.
- `trigger_us=300000`; instante de ingestão registado em ns-3: `400000 µs`.
- Codificações CPM aceites: 10, com timestamps únicos de `100000 µs` a `1000000 µs`.
- Rejeições de construção e codificação CPM: 0.
- Códigos de saída ns-3 e injector: 0.

O scheduler DENM foi deliberadamente desativado (`--enableDenm=0`). Este ensaio valida a ingestão do evento através do adaptador; não valida a submissão pelo scheduler, a transmissão DENM, a segurança nem a receção por outro nó.

### 13.4. Regressão e compilação após a alteração

Resultados executados em 2026-10-22:

| Verificação | Resultado |
|---|---|
| Testes Python CPM Green + Red | PASS — 16/16 |
| Testes Python CAM/SPATEM encontrados nas suites Green + Red | PASS — 15/15 |
| Teste da fronteira temporal SUMO/CPM | PASS — 3 verificações |
| Teste de wiring CPM do controlador | PASS |
| Teste de wiring do encoder | PASS |
| Teste da guarda de timestamp único | PASS |
| Teste UPER com round-trip e guardas semânticas | PASS |
| Compilação de `ns3.47-v2x_sumo-default` | PASS — `BUILD_RC=0` |
| `git diff --check` | PASS |

As suites CAM/SPATEM não contêm testes Python nas diretorias DENM Green e Red. A validação DENM desta atualização limita-se ao ensaio de ingestão descrito em 13.3; não se declara uma regressão completa do scheduler DENM.

A execução final de `cmake --build cmake-cache --target ns3.47-v2x_sumo-default -j2` não identificou trabalho adicional por compilar (`ninja: no work to do`, código de saída 0).

### 13.5. Interpretação, limites e decisão E2E histórica

A evidência demonstra a integração da fonte CPM com os estados de veículo processados em ns-3, a construção de mensagens e a codificação UPER para o subconjunto atualmente suportado pelo encoder. Demonstra também que a fronteira explícita do passo SUMO permite produzir uma codificação única por instante lógico observado.

Não demonstra:

- validação absoluta da georreferenciação contra um ponto de controlo geodésico independente;
- suporte integral a todos os campos e opções do perfil CPM R2;
- sincronização de relógios físicos ou validação independente da relação entre tempo lógico SUMO e um relógio ITS real;
- transmissão CPM por BTP/GeoNetworking, receção remota, segurança, interoperabilidade externa ou conformidade ETSI global;
- transmissão DENM ou regressão completa do scheduler DENM.

**Decisão: PARTIAL — a integração de construção/codificação CPM e a cadência temporal E2E estão demonstradas no âmbito testado, mas a Etapa 10.4 não fica globalmente encerrada.**

O plano global `scratch/docs/CPM/ETAPA_10_CPM_PLANO.md` permanece inalterado.


### 13.6. Arquivo e integridade dos logs E2E

Os logs brutos das execuções CPM e DENM desta secção foram preservados no
pacote local de evidência, ainda não commitado, `sumo/current/evidence/e2e-cpm-perception/20261022T133131Z/`.

O pacote contém os logs brutos do bridge e do ns-3 para o E2E CPM, os logs
do ns-3 e do injector no ensaio de ingestão DENM, um registo dos resultados
observados e o script de injector usado no ensaio controlado.

O ficheiro `SHA256SUMS` lista os hashes SHA-256 dos artefactos arquivados,
exceto o próprio manifesto. A verificação com `sha256sum -c SHA256SUMS`
deve ser executada a partir da raiz do pacote.

O arquivo dos logs preserva a evidência; não altera os limites de validade
descritos nas secções 13.3 e 13.5. O ensaio DENM demonstrou ingestão do
evento, não submissão pelo scheduler nem transmissão DENM.

## 14. Fecho final da Etapa 10.4 — 2026-10-22

**Decisão final: COMPLETE — os critérios de saída da Etapa 10.4 estão
satisfeitos para as transformações espaciais e temporais, a fonte de perceção
e o caminho de integração aqui declarados.** Esta decisão encerra a fase
10.4; não encerra a Etapa 10 CPM nem antecipa os gates de transmissão,
receção, segurança ou interoperabilidade das fases seguintes.

### 14.1. Evidência espacial independente

A fonte de produção `CpmVehiclePerceptionSource` chama
`CpmGeodeticTransform::ToEnuHorizontalRelative`. O teste geodésico Green foi
ampliado com duas referências numéricas produzidas pela ferramenta PROJ
`cct`, independente da implementação C++ usada pelo projeto. O deslocamento
para Este produziu `E=8,35920578307 m`, `N=0,00000482654 m`; para Norte,
`E=0 m`, `N=11,1062138237 m`. Ambos os casos passaram com tolerância de
0,1 mm, incluindo a ausência explícita de componente vertical.

A rede SUMO georreferenciada declara UTM zona 29/WGS84 e o respetivo
`netOffset`. O teste permanente
`test_sumo_osm_projection_reference.py` compara a coordenada geográfica do
nó OSM `11645226075` (`41,425290000`, `-8,434076200`) com a coordenada do
junction SUMO correspondente, após a projeção independente PROJ e aplicação
do offset. O erro horizontal observado foi `0,005682 m`, inferior à
tolerância de 0,05 m. O resultado é `SUMO_OSM_PROJECTION_REFERENCE=PASS`.

Esta evidência valida os eixos, a transformação horizontal utilizada pela
fonte CPM e a coerência da projeção da rede SUMO com a sua fonte OSM.
Não equivale a uma validação topográfica contra um ponto de controlo
geodésico externo independente.

### 14.2. Evidência temporal independente

A configuração CPM exige `--itsStartTime` quando o CPM está ativo. O valor é
interpretado por `vanetza::Clock::at`; a origem temporal é convertida para
microssegundos desde 2004-01-01 UTC. O teste independente usa como âncora
`2026-10-22T10:00:00Z`, equivalente a
`719748000000000 µs` nesse domínio.

O teste permanente `test_cpm_temporal_epoch_reference.py` confirma a âncora
UTC por `datetime`/`timezone` da biblioteca padrão Python e executa cinco
verificações C++: instante inicial, incremento de 100 ms, incremento de
15 s, truncagem submilissegundo e fronteira de 1 ms. A transformação
mantém validações de overflow, limites de `TimestampIts`, delta de medição
e idade do objeto.

No caminho E2E, a bridge emite `SUMO_STEP_END` com o timestamp lógico SUMO.
O adaptador entrega essa fronteira ao controlador ns-3, que usa o timestamp
recebido e a origem ITS explícita para calcular a referência e o instante
de medição CPM. A evidência arquivada mostra 150 timestamps SUMO únicos,
150 codificações CPM aceites, sem duplicados nem rejeições nesse ensaio,
com intervalo de 100 000 µs entre instantes consecutivos.

O tempo lógico SUMO é assim mapeado explicitamente para `TimestampIts`.
O relógio do scheduler ns-3 não é somado à referência temporal CPM. A
relação demonstrada com ns-3 é a entrega causal pela fronteira do passo,
não uma estimativa de sincronização ou de deriva entre relógios físicos.

### 14.3. Regressão final

As suites finais foram executadas após a adição dos testes independentes:

| Suite | Resultado |
|---|---:|
| CPM Green | PASS — 11/11 |
| CPM Red | PASS — 6/6 |
| CAM Green | PASS — 1/1 |
| CAM Red | PASS — 5/5 |
| SPATEM Green | PASS — 1/1 |
| SPATEM Red | PASS — 17/17 |
| Total | PASS — 41/41 |

O alvo `ns3.47-v2x_sumo-default` terminou com código de saída 0;
Ninja indicou `no work to do`. `git diff --check` passou. A verificação
`sha256sum -c SHA256SUMS` validou os seis artefactos do arquivo
E2E existente.

### 14.4. Limites que permanecem explícitos

A conclusão da Etapa 10.4 não afirma:

- exatidão topográfica absoluta validada contra um ponto de controlo
  geodésico independente;
- sincronização com relógios físicos externos, medição de offset/deriva
  ou observações provenientes de um sensor real;
- modelo de sensor com ruído/oclusões ou cobertura integral de todos os
  campos e opcionais do perfil CPM R2;
- transmissão CPM por BTP/GeoNetworking, receção remota, segurança, replay,
  delivery, integração completa no `PacketContext`, cobertura final do
  analyzer, interoperabilidade externa ou conformidade ETSI global;
- transmissão DENM ou regressão completa do scheduler DENM; a evidência
  desta atualização valida apenas a ingestão do evento com o scheduler
  deliberadamente desativado.

### 14.5. Estado dos artefactos

O plano global `scratch/docs/CPM/ETAPA_10_CPM_PLANO.md` permanece inalterado.
O relatório, os testes adicionados e o arquivo de evidência continuam no
working tree e não foram commitados. O fecho funcional desta fase não
constitui commit nem valida as fases posteriores da Etapa 10 CPM.
