---
title: "P0 — Rebaseline do Checkout Atual — 2026-10-09"
status: "BASELINE_CAPTURED_WITH_KNOWN_LIMITATIONS"
type: "operational-rebaseline"
date: "2026-10-09"
project: "ns-3 + Vanetza + SUMO V2X"
branch: "feature/spatem-current-clean-2026-10-09"
baseline_commit: "7dbf6278c22db047c6701c82cdadab1b20fe3026"
tags:
  - ns3
  - vanetza
  - sumo
  - spatem
  - cam
  - denm
  - p0
  - rebaseline
  - validation
related:
  - "[[CURRENT_STATE]]"
  - "[[DOCUMENT_STATUS]]"
  - "[[ETAPA_9_SPATEM_PLANO]]"
---

# P0 — Rebaseline do Checkout Atual

## 1. Decisão

A baseline operacional foi recolhida e registada. O checkout, a compilação,
os resultados CAM/DENM e P21 foram verificados. A baseline não é declarada
globalmente verde: os resultados SPATEM abaixo incluem duas limitações de
aceitação que permanecem por resolver ou reproduzir.

Princípio de validação:

> Código plausível não é comportamento demonstrado; build limpo não é
> validação funcional; execução local não é interoperabilidade externa.

## 2. Identidade Git

| Campo | Resultado |
|---|---|
| Branch | `feature/spatem-current-clean-2026-10-09` |
| HEAD | `7dbf6278c22db047c6701c82cdadab1b20fe3026` |
| Commit | `wip(sumo): checkpoint UDP receive buffer change` |
| Upstream | `origin/feature/spatem-current-clean-2026-10-09` |
| Divergência local/remota | `0 / 0` |
| Working tree no início e fim da recolha | CLEAN |
| Tag diretamente em HEAD | Nenhuma |
| Tag mais próxima | `etapa-9-p14-security-negative-tests-2026-09-29` |
| Distância indicada por `git describe` | 40 commits até `7dbf627` |

O checkpoint `7dbf627` preserva uma alteração de configuração `SO_RCVBUF`
em `sumo_socket.cc`. Esse commit é um checkpoint de código e não constitui,
por si só, prova de validação runtime dessa alteração.

## 3. Build

| Verificação | Resultado |
|---|---|
| `./ns3 build` | PASS |
| Código de saída | `0` |

Evidência local recolhida em:
`/tmp/spatem-current-validation/build.log`

O build confirma compilação e ligação dos alvos incluídos na configuração.
Não substitui os testes de aceitação runtime.

## 4. Matriz de aceitação

| Área | Resultado | Evidência |
|---|---|---|
| DENM bridge | PASS | 5 testes, 0 falhas |
| CAM-only, Etapa 8.13 | PASS | `RESULTADO 8.13: PASS`, `ACCEPTANCE_EXIT=0` |
| CAM + DENM, P19 | PASS | `P19_ACCEPTANCE_EXIT=0` |
| P21.1 — DENM lifecycle | PASS | `P21_1_LIFECYCLE_ACCEPTANCE_EXIT=0` |
| P21.2 — analyzer | PASS | `P21_2_ANALYZER_ACCEPTANCE_EXIT=0` |
| P21.3 — DCC/credenciais | PASS | `P21_3_DCC_CREDENTIAL_ROTATION_ACCEPTANCE_EXIT=0` |
| P21.4 — alta densidade/bridge | PASS | `P21_4_HIGH_DENSITY_BRIDGE_ACCEPTANCE_EXIT=0` |
| P21.5 — identidade/rotação | PASS | `P21_5_IDENTITY_CREDENTIAL_ROTATION_ACCEPTANCE_EXIT=0` |
| P21.6 — GBC inválido | PASS | `P21_6_INVALID_GBC_ACCEPTANCE_EXIT=0` |
| SPATEM analyzer | PASS | `test_spatem_analyzer_acceptance` |
| SPATEM TX/PacketContext | PASS | `test_spatem_real_tx_context_acceptance` |
| SPATEM RX pipeline | FAIL | `VERIFY_OK=1`, `DECODE_OK=0`, sem REPLAY/RECEPTION/DELIVER |
| SPATEM RX real | PASS | `test_spatem_real_rx_acceptance` |
| SPATEM replay real | PASS | `test_spatem_real_replay_acceptance` |
| SPATEM SUMO/TraCI | BLOCKED / FAIL | Configuração SUMO referenciada não encontrada |
| DENM E2E histórico 8.12 | NOT REPRODUCED | Log `output_log_etapa_8_12.txt` ausente |

Os primeiros invocations dos validadores CAM devolveram `RC=2` por falta
de argumentos. A execução posterior com os argumentos corretos passou.
Esses erros de invocação não são classificados como falhas funcionais CAM.

## 5. Evidência CAM-only

Log validado:
`test-results/p9_13/p9_13_cam_16s.log`

- 611 pacotes CAM identificados no TX.
- 607 transmissões completadas.
- 4 transmissões pendentes no horizonte DCC, contabilizadas explicitamente.
- Cadeia RX observada desde `RAW_RX` até `DELIVER_OK`.
- BTP port `2001`, SHB, ITS-AID `36 / CA`, perfil `ITS_G5`.
- Nenhum evento DENM/GBC nesse cenário.

Resultado formal: `RESULTADO 8.13: PASS`.

## 6. Evidência CAM + DENM

Log integrado validado:
`test-results/p21/p21_4_bridge_ns3_clean.log`

A aceitação P19 passou, com CAM e DENM ativos no mesmo log. O validador
não detetou falhas de encode, signing, verification, decode ou send.
Os controlos negativos de replay e self-origin não identificaram pacotes
rejeitados que tivessem chegado a reception/delivery.

Resultado formal: `RESULTADO P19: PASS`.

Esta verificação validou um log de runtime existente; não significa que
uma nova simulação integrada tenha sido executada durante a recolha P0.

## 7. Limitações SPATEM observadas

### 7.1 RX pipeline

O teste real observou:
`SOCKET_RX=2`, `SPATEM_RX_DISPATCH=1`, `SPATEM_BTP_RX_OK=1` e
`SPATEM_VERIFY_OK=1`, mas `SPATEM_DECODE_OK=0`. Os eventos posteriores
de replay, reception e delivery ficaram ausentes.

O executável terminou com código zero, mas a aceitação falhou. O motivo
exato da falta de descodificação não foi determinado no P0.

### 7.2 SUMO/TraCI

O publisher tentou abrir:

`/tmp/etapa9_13_tls/simple.sumocfg`

O SUMO devolveu `Could not access configuration`; o publisher terminou
com erro TraCI e o adapter observou zero callbacks. Esta execução não
demonstra uma falha funcional do mapper SPATEM: o cenário necessário
não estava acessível.

### 7.3 DENM E2E histórico 8.12

O acceptance runner não encontrou `output_log_etapa_8_12.txt`.
A validação histórica não foi reproduzida nesta recolha. Isso não é
classificado como uma nova falha funcional DENM.

## 8. Evidência local

Os registos produzidos durante a recolha encontram-se em:

- `/tmp/p0-rebaseline-2026-10-09/tests.tsv`
- `/tmp/p0-rebaseline-2026-10-09/cam_acceptance.log`
- `/tmp/p0-rebaseline-2026-10-09/cam_denm_acceptance.log`
- `/tmp/spatem-current-validation/summary.tsv`
- `/tmp/spatem-current-validation/build.log`

Os caminhos em `/tmp` são evidência local e não são garantidos como
artefactos versionados do repositório.

## 9. Fecho do P0

**RESULTADO: BASELINE CAPTURADA COM LIMITAÇÕES EXPLICITAMENTE REGISTADAS.**

A baseline identifica commit, branch, tags, build e testes de referência.
As duas falhas SPATEM continuam abertas para investigação numa etapa
posterior. Não se declara a suite SPATEM integralmente aprovada.

Próxima atividade: seguir a etapa técnica aprovada no plano SPATEM,
mantendo as limitações desta baseline visíveis e sem alterar
retroativamente o histórico P21.

## Relações

- [[CURRENT_STATE]]
- [[DOCUMENT_STATUS]]
- [[ETAPA_9_SPATEM_PLANO]]
- [[ETAPA_9_SPATEM_REBASELINE_2026-10-08]]
