---
title: P17 — P4 Bridge / SUMO Integration
type: validation-state
status: VALIDATED
date: 2026-10-02
project: ns-3 + Vanetza + SUMO V2X
phase: Etapa 9
tags:
  - ns3
  - vanetza
  - sumo
  - denm
  - p17
  - validation
---

# P17 — P4 Bridge / SUMO Integration

## Status

**STATUS:** VALIDATED
**RESULT:** PASS

P17 validou a integração operacional entre a população SUMO, a bridge `p4_bridge_high_density.py` e o runtime ns-3/Vanetza para ingestão de eventos DENM.

## Evidence

`test-results/p17/p17_ns3.log`

`test-results/p17/p17_p4_bridge.log`

[[scratch/vanetza_integration/tests/test_p17_p4_bridge_integration.sh]]

Acceptance commit: `ac3dd63`

## Correlation

```text
bridge_events=7
ns3_ingest_events=7
matched=7
missing=0
unexpected=0
```

Os eventos corresponderam aos veículos `veh0` a `veh6`, preservando o timestamp do evento entre a bridge e a ingestão no ns-3.

## DENM pipeline

```text
GBC_REQUEST
GBC_REQUEST_OK
DENM_RECEPTION_OK
DENM_DELIVER_OK
```

## Scope

P17 fecha o limite que permanecia aberto no P16: integração operacional entre SUMO, identidade do veículo e runtime ns-3/Vanetza.

Esta evidência é de execução integrada local ns-3 + Vanetza + SUMO e não constitui validação geral de interoperabilidade externa.

## Relations

[[scratch/vanetza_integration/docs/P16_High_Density_DENM_Provisioning]]
[[scratch/vanetza_integration/docs/P18_Credential_Binding_Ownership]]
[[scratch/vanetza_integration/docs/P19_CAM_DENM_Regression_Acceptance]]
[[scratch/vanetza_integration/docs/P20_High_Density_V3_Runtime_Validation]]
[[scratch/vanetza_integration/docs/CURRENT_STATE]]
[[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
