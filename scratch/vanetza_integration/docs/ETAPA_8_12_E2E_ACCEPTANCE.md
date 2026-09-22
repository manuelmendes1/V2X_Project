---
status: completed
type: acceptance-test
date: 2026-09-22
project: ns-3-vanetza-sumo
stage: "8.12"
result: PASS
commit: 78cd7a4
test: scratch/vanetza_integration/tests/test_denm_e2e_acceptance.sh
runtime_log: output_log_etapa_8_12.txt
tags:
  - ns3
  - vanetza
  - sumo
  - v2x
  - denm
  - e2e
  - acceptance
  - validation
  - etapa-8
  - etapa-8-12
related:
  - [[scratch/vanetza_integration/docs/CURRENT_STATE]]
  - [[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
  - [[scratch/v2x_analyzer/README]]
---

# Etapa 8.12 — DENM End-to-End Acceptance

**STATUS:** PASS  
**DATE:** 2026-09-22  
**PROJECT:** ns-3.47 + Vanetza + SUMO V2X Integration

## 1. Objetivo

Formalizar a aceitação da fase 8.12 através de uma execução runtime real.

O critério exige pelo menos uma cadeia DENM válida entre um originador e um receptor distinto, atravessando o caminho TX e RX real.

## 2. Teste formal

Script:

`scratch/vanetza_integration/tests/test_denm_e2e_acceptance.sh`

Commit:

`78cd7a4 test(denm): add formal 8.12 end-to-end acceptance`

## 3. Execução runtime

```text
RUN_EXIT=0
```

Teste de aceitação:

`./scratch/vanetza_integration/tests/test_denm_e2e_acceptance.sh output_log_etapa_8_12.txt`

Resultado formal:

```text
RESULTADO 8.12: PASS
ACCEPTANCE_EXIT=0
```

## 4. Caso E2E aceite

```text
TX: veh1 / station=2 / PKT=9 / action_seq=1
RX: PKT=11 / origin_station=2 / receiver_station=1 / action_seq=1
```

## 5. Caminho TX demonstrado

```text
GBC_REQUEST
    port=2002
    aid=37
    transport=GBC
        ↓
BTP_TX_BEGIN
        ↓
BTP_TX_OK
        ↓
GEONET_TX_BEGIN
        ↓
GEONET_TX_OK
        ↓
ACCESS_TX_BEGIN
        ↓
ACCESS_TX_OK
        ↓
SEND_BEGIN
        ↓
SEND_OK
        ↓
TX_COMPLETE
```

## 6. Caminho RX demonstrado

```text
DENM_RX_DISPATCH
        ↓
GEONET_RX_OK
        ↓
BTP_RX_OK port=2002
        ↓
VERIFY_OK
        ↓
DENM_GN_SOURCE
        ↓
DENM_DECODE_OK
        ↓
DENM_REPLAY_OK
        ↓
DENM_RECEPTION_OK
        ↓
DENM_DELIVER_OK
```

## 7. Critérios negativos

```text
DENM validation failures = 0
DENM replay failures     = 0
DENM_SELF_ORIGIN_DROP    = 5
```

Os `DENM_SELF_ORIGIN_DROP` observados não invalidam a aceitação. Correspondem ao filtro esperado de mensagens GBC que regressam ao próprio originador.

## 8. Identidade e correlação

```text
ActionID        = identidade lógica do evento DENM
packetId        = identidade do pacote ns-3
GN source MID   = origem GeoNetworking
immediate sender = último hop observado
receiver        = estação que processa o evento
```

No caso aceite:

```text
origin_station = 2
receiver_station = 1
action_seq = 1
```

## 9. Critérios formais

| Critério | Evidência | Estado |
|---|---|---|
| DENM request | GBC_REQUEST | PASS |
| BTP-B | port=2002 | PASS |
| ITS-AID | aid=37 / DEN | PASS |
| GeoNetworking | transport=GBC | PASS |
| TX completion | TX_COMPLETE | PASS |
| RX dispatch | DENM_RX_DISPATCH | PASS |
| GeoNetworking RX | GEONET_RX_OK | PASS |
| BTP RX | BTP_RX_OK port=2002 | PASS |
| Verification | VERIFY_OK | PASS |
| GN source | DENM_GN_SOURCE | PASS |
| Decode | DENM_DECODE_OK | PASS |
| Replay | DENM_REPLAY_OK | PASS |
| Reception | DENM_RECEPTION_OK | PASS |
| Delivery | DENM_DELIVER_OK | PASS |
| Origin != receiver | 2 != 1 | PASS |

## 10. Resultado

```text
8.12 — End-to-End
STATUS = COMPLETE
RESULT = PASS
ACCEPTANCE_EXIT = 0
```

## 11. Relações documentais

- [[scratch/vanetza_integration/docs/CURRENT_STATE]]
- [[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
- [[scratch/v2x_analyzer/README]]

Este documento preserva a evidência posterior ao snapshot histórico associado a `ee2da3a`. O snapshot histórico não é substituído.

## 12. Artefactos associados

```text
Test: scratch/vanetza_integration/tests/test_denm_e2e_acceptance.sh
Log:  output_log_etapa_8_12.txt
Git:  78cd7a4
```

## 13. Estado após 8.12

```text
8.11  v2x_analyzer DENM extension   COMPLETE
8.12  DENM E2E acceptance           COMPLETE / PASS
8.13  CAM regression                TO CONSOLIDATE
8.14  Full Etapa 8 closeout         PENDING
```
