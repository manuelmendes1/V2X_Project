---
title: "Etapa 8.13 — CAM Regression Acceptance"
status: "PASS"
date: "2026-09-22"
revalidated: "2026-10-08"
project: "ns-3 + Vanetza + SUMO V2X"
stage: "8.13"
type: "validation"
tags:
  - v2x
  - cam
  - regression
  - validation
  - acceptance
  - ns3
  - vanetza
  - sumo
  - etapa-8
  - etapa-8-13
evidence_log: "../../../test-results/p9_13/p9_13_cam_16s.log"
test_script: "../tests/test_cam_regression_acceptance.sh"
---

# Etapa 8.13 — CAM Regression Acceptance

**STATUS:** COMPLETED
**DATE:** 2026-09-22
**RESULT:** PASS

## Navegação

- [[CURRENT_STATE]]
- [[DOCUMENT_STATUS]]
- [[ETAPA_8_12_E2E_ACCEPTANCE]]
- [[ETAPA_8_13_CAM_REGRESSION]]
- [[ETAPA_9_SPATEM_REBASELINE_2026-10-08]]

Teste formal:

[[../tests/test_cam_regression_acceptance.sh]]

Evidência runtime:

`test-results/p9_13/p9_13_cam_16s.log`

## Objetivo

A Etapa 8.13 valida a regressão CAM após a introdução do DENM, executando a simulação com DENM explicitamente desativado.

## Execução

```bash
./build/scratch/ns3.47-v2x_sumo-default \
  --enableDenm=0 \
  --simDuration=16 \
  > test-results/p9_13/p9_13_cam_16s.log 2>&1
```

Resultado da execução:

```text
RUN_EXIT=0
```

## CAM TX

Foram observadas 75 ocorrências em cada estágio instrumentado:

```text
BTP_TX_BEGIN
BTP_TX_OK
GEONET_TX_BEGIN
GEONET_TX_OK
ACCESS_TX_BEGIN
ACCESS_TX_OK
SEND_BEGIN
SEND_OK
TX_COMPLETE
```

## CAM RX

Na execução revalidada, a cadeia CAM RX foi observada com uma ocorrência em cada estágio instrumentado, desde `RAW_RX` até `DELIVER_OK`:

```text
RAW_RX
ACCESS_RX_BEGIN
ACCESS_RX_OK
GEONET_RX_BEGIN
GEONET_RX_OK
BTP_RX_BEGIN
BTP_RX_OK
VERIFY_BEGIN
VERIFY_OK
DECODE_OK
REPLAY_OK
RECEPTION_BEGIN
RECEPTION_OK
DELIVER_BEGIN
DELIVER_OK
```

## Identidade CAM

A execução confirmou a identidade CAM:

```text
BTP port = 2001
transport = SHB
ITS-AID = 36 / CA
profile = ITS_G5
```

O RX confirmou igualmente `BTP_RX_OK` com `port=2001`.

## Instrumentação ACCESS_RX

O estágio Access RX é atualmente instrumentado através de dois eventos:

```text
ACCESS_RX_BEGIN = 133
ACCESS_RX_OK    = 133
```

Portanto, o pipeline RX mantém a sequência:

```text
RAW_RX
-> ACCESS_RX
-> GEONET_RX
-> BTP_RX
-> VERIFY
-> DECODE
-> REPLAY
-> RECEPTION
-> DELIVER
```

## Ausência de DENM

Com `--enableDenm=0`, não foram observados eventos DENM/GBC.

```text
GBC_REQUEST            = 0
GBC_REQUEST_OK         = 0
DENM_RX_DISPATCH       = 0
DENM_DECODE_OK         = 0
DENM_REPLAY_OK         = 0
DENM_REPLAY_DROP       = 0
DENM_RECEPTION_OK      = 0
DENM_DELIVER_OK        = 0
DENM_SELF_ORIGIN_DROP  = 0
```

## Resultado formal

O acceptance formal foi executado sobre o log da regressão CAM:

```bash
./scratch/vanetza_integration/tests/test_cam_regression_acceptance.sh \
  test-results/p9_13/p9_13_cam_16s.log \
  0 \
  16
```

Resultado:

```text
RESULTADO 8.13: PASS
ACCEPTANCE_EXIT=0
```

## Interpretação

A aceitação demonstra continuidade funcional do pipeline CAM TX e RX, preservação da identidade CAM e ausência de atividade DENM/GBC durante uma execução com DENM desativado.

A aceitação atual distingue entre pacotes CAM completamente transmitidos antes do
fim da simulação e pacotes legitimamente pendentes no horizonte devido ao agendamento DCC.

Na execução de 16 s foram observados 611 CAM TX:
607 completados integralmente e 4 pendentes no horizonte
(PKT=609,610,611,612), com accounting terminal 607 + 4 = 611.

A regressão possui ainda um controlo negativo: remover um TX_COMPLETE de um
pacote que já passou pelo DCC faz a acceptance falhar.

Esta aceitação não demonstra interoperabilidade externa nem constitui uma medição independente de perda rádio.

## Relações

Esta etapa complementa [[ETAPA_8_12_E2E_ACCEPTANCE]], que validou o ciclo E2E DENM.

Estado corrente: [[CURRENT_STATE]]

Índice documental: [[DOCUMENT_STATUS]]

Análise: [[../../v2x_analyzer/README]]

Próxima etapa: [[ETAPA_9_SPATEM_REBASELINE_2026-10-08]]
