---
title: P20 — High-Density DENM V3 Runtime Validation
type: runtime-validation
status: VALIDATED
date: 2026-10-02
project: ns-3 + Vanetza + SUMO V2X
phase: Etapa 9
tags:
  - ns3
  - vanetza
  - denm
  - pki
  - high-density
  - credential-rotation
  - p20
---

# P20 — High-Density DENM V3 Runtime Validation

## Status

**STATUS:** VALIDATED
**RESULT:** PASS

P20 validou criação, inicialização e assinatura DENM V3 em runtime para múltiplos veículos usando credenciais DENM provisionadas.

## Formal envelope

```text
10 vehicles = PASS
20 vehicles = PASS
```

Evidência: `test-results/p20/p20_post_p18_10.log` e `test-results/p20/p20_post_p18_20.log`.

Ambas terminaram com `DENM_HIGH_DENSITY_V3_RUNTIME_SIGN_EXIT=0`.

## Extended envelope

A extensão com 100 veículos produziu:

```text
VEHICLE_COUNT=100
UNIQUE_SIGNER_COUNT=100
DENM_HIGH_DENSITY_V3_RUNTIME_SIGN_EXIT=0
```

Evidência: `test-results/p20/p20_post_p18_100.log`.

## Credential rotation

A rotação para 100 veículos e três credenciais produziu:

```text
VEHICLE_COUNT=100
UNIQUE_GLOBAL_SIGNER_COUNT=300
DENM_HIGH_DENSITY_V3_RUNTIME_ROTATION_EXIT=0
```

Evidência: `test-results/p20/p20_post_p18_100_rotation.log`.

As transições `credential 000 -> 001` e `credential 001 -> 002` foram demonstradas para os 100 veículos.

## Scope

A evidência demonstra high-density vehicle creation, V3 signing em runtime e credential rotation. Não estabelece interoperabilidade PKI ou V2X externa.

P20 não exigiu novo commit de implementação.

## Relations

[[scratch/vanetza_integration/docs/P16_High_Density_DENM_Provisioning]]
[[scratch/vanetza_integration/docs/P17_P4_Bridge_Integration]]
[[scratch/vanetza_integration/docs/P18_Credential_Binding_Ownership]]
[[scratch/vanetza_integration/docs/P19_CAM_DENM_Regression_Acceptance]]
[[scratch/vanetza_integration/docs/CURRENT_STATE]]
[[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
