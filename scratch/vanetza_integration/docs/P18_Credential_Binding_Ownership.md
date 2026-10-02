---
title: P18 — DENM Credential Binding Ownership
type: security-validation
status: VALIDATED
date: 2026-10-02
project: ns-3 + Vanetza + SUMO V2X
phase: Etapa 9
tags:
  - ns3
  - vanetza
  - denm
  - security
  - credential-binding
  - p18
---

# P18 — DENM Credential Binding Ownership

## Status

**STATUS:** VALIDATED
**RESULT:** PASS

P18 estabelece que um certificado ETSI V3 somente pode tornar-se credencial DENM ativa depois da validação da correspondência entre certificado e chave privada.

## Implementation

`VehicleSecurityContext::SetActiveDenmCredential()` utiliza `VehicleCredentialBindingValidator::Validate(...)` antes da alteração da credencial ativa.

## Regression

[[scratch/denm_runtime_atomic_rotation_test/denm_runtime_atomic_rotation_test.cc]]

O teste verifica rejeição do par certificado/chave inválido e preservação do active credential index, certificate path e private-key path.

## Evidence

Commit: `6807573`

O teste focado e a compilação completa foram validados após a alteração.

## Relations

[[scratch/vanetza_integration/docs/P17_P4_Bridge_Integration]]
[[scratch/vanetza_integration/docs/P19_CAM_DENM_Regression_Acceptance]]
[[scratch/vanetza_integration/docs/P20_High_Density_V3_Runtime_Validation]]
[[scratch/vanetza_integration/docs/CURRENT_STATE]]
[[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
