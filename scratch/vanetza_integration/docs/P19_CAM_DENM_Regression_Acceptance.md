---
title: P19 — CAM / DENM Regression Acceptance
type: acceptance-validation
status: VALIDATED
date: 2026-10-02
project: ns-3 + Vanetza + SUMO V2X
phase: Etapa 9
tags:
  - ns3
  - vanetza
  - sumo
  - cam
  - denm
  - regression
  - p19
---

# P19 — CAM / DENM Regression Acceptance

## Status

**STATUS:** VALIDATED
**RESULT:** PASS

P19 estabeleceu uma aceitação conjunta CAM + DENM após as alterações de segurança e credenciais.

## Evidence

[[scratch/vanetza_integration/tests/test_cam_denm_regression_acceptance.sh]]

`test-results/p19/p19_ns3_post_p18.log`

`test-results/p19/p19_p4_bridge_post_p18.log`

Acceptance commit: `4b4ef28`

```text
RUN_EXIT=0
P19_ACCEPTANCE_EXIT=0
```

## CAM

```text
BTP port = 2001
transport = SHB
ITS-AID = 36
GeoNetworking profile = ITS_G5
AID = CA
```

TX atingiu `TX_COMPLETE` e o RX atingiu BTP_RX, DECODE, REPLAY, RECEPTION e DELIVER.

## DENM

```text
BTP port = 2002
transport = GBC
ITS-AID = 37
```

RX atingiu DENM_RX_DISPATCH, DENM_GN_SOURCE, DENM_DECODE_OK, DENM_REPLAY_OK, DENM_RECEPTION_OK e DENM_DELIVER_OK.

Auditoria pós-P18:

```text
GBC_REQUEST=50
GBC_REQUEST_OK=25
DENM_RECEPTION_OK=36
DENM_DELIVER_OK=36
DENM_REPLAY_DROP=672
DENM_SELF_ORIGIN_DROP=125
```

## Scope

P19 é uma aceitação de regressão do runtime integrado CAM + DENM. Não substitui interoperabilidade externa.

## Relations

[[scratch/vanetza_integration/docs/P17_P4_Bridge_Integration]]
[[scratch/vanetza_integration/docs/P18_Credential_Binding_Ownership]]
[[scratch/vanetza_integration/docs/P20_High_Density_V3_Runtime_Validation]]
[[scratch/vanetza_integration/docs/CURRENT_STATE]]
[[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
