---
title: "Etapa 10.14 — Integração SUMO como fonte de perceção CPM"
status: "VALIDATED"
type: "E2E INTEGRATION AND VALIDATION EVIDENCE"
date: "2026-10-28"
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
stage: "ETAPA 10.14"
branch: "feature/cpm-etapa10-14-sumo-perception-e2e"
observed_head_commit: "de10084f9ccbee51229d3ab87978764b95483d18"
overall_cpm_closeout: "NOT CLOSED"
---

# Etapa 10.14 — Integração SUMO como fonte de perceção CPM

## 1. Estado

**Estado: VALIDATED — critérios E2E documentados da Etapa 10.14 aprovados.**

A validação inclui a fonte determinística de perceção alimentada pela cena SUMO, geração e codificação CPM, transmissão através de ns-3/Vanetza, receção, verificação de segurança, descodificação e entrega à aplicação.

Na execução temporal de 2026-10-28, 131 CPM foram codificados e 78 foram descodificados. Nos 78 packet IDs comuns, a referência temporal CPM, os conjuntos de `objectId`, `measurement_delta_ms` e `object_age_ms` não apresentam divergências TX/RX.

Na auditoria espacial da mesma execução, as 78 comparações disponíveis apresentam referências WGS84 coincidentes, diferenças horizontais máximas de 0,01 m e fingerprints UPER coincidentes.

Esta validação está limitada ao cenário e à configuração documentados. Não fecha a Etapa 10 global, não demonstra interoperabilidade OTA nem certifica conformidade ETSI integral.

## 2. Cenário executado

- Configuração SUMO: `sumo/current/scenario/georeferenced/vila-nova-de-famalicao/famalicao-demo.sumocfg`
- Bridge: `sumo/current/integration/v2x_bridge_16.py`
- Diretório da execução temporal: `/tmp/cpm-etapa10-14-temporal-e2e.OjFgfN`
- Log ns-3: `/tmp/cpm-etapa10-14-temporal-e2e.OjFgfN/ns3.log`
- Log bridge: `/tmp/cpm-etapa10-14-temporal-e2e.OjFgfN/bridge.log`
- ITS start time: `2026-10-28T10:24:34`
- Diretório da execução espacial inicial: `/tmp/cpm-etapa10-14-coordinate-e2e.rHUYUm`
- Estação RSU emissora: stationId 9001
- Posição projetada RSU: X=20399.636 m, Y=5152.868 m
- Referência geográfica RSU: latitude=41.3848689°, longitude=-8.4341234°
- Alcance horizontal de perceção: 90 m
- Passo SUMO: 0,1 s
- Duração SUMO: 16 s
- Código de saída da bridge e do runtime ns-3 na execução temporal: 0 e 0.

A caracterização da trajetória da execução espacial inicial registou distância inicial/final aproximada de 93,36 m, distância mínima de 67,38 m, entrada no alcance por volta de 2,1 s, saída por volta de 15,3 s e 132 amostras dentro do alcance.

A fonte produziu mensagens enquanto existiam objetos elegíveis e omitiu transmissões quando não havia objetos elegíveis (`TX_SKIPPED reason=NO_PERCEIVED_OBJECTS`). Isto valida o comportamento da fonte determinística alimentada pelo SUMO, não um modelo sensorial realista de câmara, radar ou lidar.

## 3. Contagens do percurso E2E

| Métrica | Resultado |
|---|---:|
| CPM gerados | 131 |
| CPM codificados | 131 |
| Inícios de TX BTP | 131 |
| Packet IDs TX distintos | 131 |
| Packet IDs RX distintos | 78 |
| Packet IDs comuns TX/RX | 78 |
| Packet IDs TX sem RX correspondente | 53 |
| CPM descodificados | 78 |
| CPM aceites pelo controlo de replay | 78 |
| CPM publicados na receção | 78 |
| CPM entregues à aplicação | 78 |

Todos os 78 pacotes descodificados têm um TX correspondente; não foram encontrados packet IDs RX sem TX correspondente. Os 53 packet IDs TX sem receção correspondente não foram tratados como falhas de preservação temporal.

Na execução espacial inicial, os contadores PHY mostraram 131 inícios de TX, 78 eventos `RX_END` e 53 descartes físicos: 53/131 = 40,46%. A execução temporal registou igualmente 131 TX e 78 RX por correspondência dos packet IDs. As perdas do canal devem permanecer separadas das validações de descodificação, integridade e coerência temporal.

## 4. Coerência espacial e integridade dos dados TX/RX

| Verificação | Resultado |
|---|---:|
| Registos TX com coordenadas | 131 |
| Registos RX com coordenadas | 78 |
| Packet IDs comuns TX/RX | 78 |
| Diferença absoluta máxima de latitude da referência | 0,0000000000° |
| Diferença absoluta máxima de longitude da referência | 0,0000000000° |
| Diferença absoluta máxima em Este | 0,01 m |
| Diferença absoluta máxima em Norte | 0,01 m |
| Conjuntos de `objectId` divergentes | 0 |
| Problemas nos registos de posição | 0 |
| Fingerprints UPER comparados | 78 |
| Fingerprints UPER coincidentes | 78 |
| Fingerprints UPER divergentes | 0 |

As diferenças horizontais foram calculadas a partir das posições registadas com três casas decimais. O máximo observado de 0,01 m fica dentro da tolerância de auditoria de 0,011 m. A referência WGS84 coincide nos valores registados.

Os 78 fingerprints UPER coincidentes demonstram igualdade dos payloads comparados nos pacotes recebidos. Estes resultados validam a coerência TX/RX observada no cenário; não certificam a precisão geodésica absoluta da rede.

## 5. Validação temporal

**Resultado: PASS.** Auditoria executada sobre `/tmp/cpm-etapa10-14-temporal-e2e.OjFgfN/ns3.log`.

| Verificação temporal | Resultado |
|---|---:|
| Registos `BUILD_ACCEPTED` | 131 |
| Registos BUILD com tempo SUMO e referência ITS | 131 |
| Referências em `CPM_GENERATE_OK` | 131 |
| Correspondência BUILD → GENERATE pela ordem de produção | 131/131 |
| Offset SUMO → ITS constante | Sim |
| Offset observado `reference_time_its_ms - floor(reference_sumo_us / 1000)` | 720267874000 ms |
| Divergências de `reference_time_its_ms` TX/RX | 0 |
| Divergências dos conjuntos de `objectId` TX/RX | 0 |
| Divergências de `measurement_delta_ms` / `object_age_ms` TX/RX | 0 |

Para os 78 packet IDs comuns TX/RX, a referência temporal CPM coincide. Para todos os objetos desses pacotes, `measurement_delta_ms` e `object_age_ms` são iguais nos resumos TX e RX.

O campo `object_temporal_ms` associa cada `objectId` ao delta de medição e à idade do objeto. Quando a idade opcional não está presente, é registado `unavailable`, distinguindo-a de zero.

**Limitação de correlação:** `BUILD_ACCEPTED` não fornece `packet_id`. A correspondência BUILD/GENERATE foi verificada pela ordem de produção, com 131 registos em cada lado, e não por uma chave de pacote direta. A comparação dos campos temporais dos objetos foi feita diretamente por `packet_id` e `objectId` nos pacotes TX/RX comuns.

O offset constante caracteriza o mapeamento observado nesta execução. Não demonstra sincronização de relógios com sistemas externos.

## 6. Testes e build

| Verificação | Resultado |
|---|---|
| Build limpo de `ns3.47-v2x_sumo-default` | PASS |
| Suite oficial Green + Red | PASS — 96 testes, zero falhas e zero erros |
| Testes focados de logging CPM | PASS — 8/8 |
| Testes do analyzer | PASS — 11/11 |
| `git diff --check` | PASS |
| Runtime da execução temporal SUMO/ns-3 | PASS — códigos de saída 0/0 |
| Auditoria temporal por pacote e objeto | PASS |
| Auditoria espacial e fingerprints UPER | PASS |

A suite oficial foi executada com `python3 scratch/test/run_tests.py --suite all`. Os testes do analyzer passaram no ambiente temporário Python 3.14, usando `PYTHONPATH` com `scratch`. O ambiente original `ns3-venv` continua a referenciar Python 3.12, que já não existe no sistema; não foi reparado nesta atividade.

Os resultados validam o cenário e os testes executados. Não demonstram interoperabilidade OTA, conformidade ETSI integral ou desempenho noutras topologias.

## 7. Alterações de código associadas

Os quatro ficheiros rastreados alterados nesta revisão são:

- `scratch/test/Red/CPM/test_cpm_packet_context_logging.py` — testes de contrato para logging temporal por objeto em TX e RX.
- `scratch/test/Red/SPATEM/test_spatem_physical_duplicate_hook.py` — expectativa textual atualizada para incluir `duplicateCpm`, uma captura já presente na implementação.
- `scratch/vanetza_integration/cpm/service/cpm_service.cc` — registo TX dos campos temporais por objeto.
- `scratch/vanetza_integration/cpm/rx/cpm_rx_pipeline.cc` — registo RX dos campos temporais por objeto.

As alterações CPM acrescentam `object_temporal_ms` aos resumos de logging. Não foram concebidas para alterar a codificação UPER, a assinatura, o transporte ou a lógica de entrega.

Este relatório continua a ser um artefacto local ignorado pelo Git. Não foi necessário modificar o `.gitignore` nem adicionar o relatório à indexação do Git.

## 8. Matriz de aceitação da fase

| Critério | Estado | Evidência ou limite |
|---|---|---|
| Fonte SUMO alimenta a perceção determinística | PASS | CPM produzidos para objetos elegíveis |
| Comportamento de entrada/saída do alcance | PASS | Trajetória caracterizada na execução espacial |
| Referência WGS84 preservada TX/RX | PASS | Diferença máxima observada 0,0000000000° |
| Posições ENU horizontais coerentes | PASS | Diferença máxima de 0,01 m |
| Conjuntos de identificadores de objeto preservados | PASS | 0 divergências nos 78 packet IDs comuns |
| Integridade dos payloads UPER | PASS | 78 fingerprints coincidentes, 0 divergências |
| Mapeamento temporal SUMO → ITS | PASS | 131/131 referências BUILD/GENERATE coincidentes pela ordem; offset constante |
| Delta de medição e idade do objeto preservados | PASS | 0 divergências nos objetos dos 78 pacotes RX |
| Descodificação e entrega CPM | PASS | 78 descodificados e entregues |
| Canal físico sem perdas | NÃO DEMONSTRADO | Foram observadas 53 ausências de RX por 131 TX na execução espacial inicial |
| Modelo sensorial realista | FORA DO ÂMBITO | Fonte determinística alimentada pelo SUMO |
| Interoperabilidade OTA e conformidade ETSI integral | FORA DO ÂMBITO | Não demonstradas por esta fase |

**Decisão da fase:** VALIDATED para os critérios E2E documentados da Etapa 10.14. Esta decisão é restrita ao cenário, à configuração e às evidências registadas; não fecha a Etapa 10 global.

## 9. Conclusão e próximo gate

A integração funcional SUMO/CPM está demonstrada no cenário controlado. A coerência espacial e dos fingerprints para os 78 pacotes recebidos está confirmada. As perdas físicas permanecem explícitas e não devem ser omitidas.

A fase fica PARTIAL até a correlação temporal por pacote estar auditada e documentada. As fases de robustez, tamanho/segmentação, regressão e closeout global continuam sujeitas aos respetivos gates.

O plano geral `ETAPA_10_CPM_PLANO.md` mantém-se inalterado e local. Este relatório não declara a Etapa 10 global concluída nem regista qualquer commit.
