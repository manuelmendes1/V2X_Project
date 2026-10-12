---
title: "Etapa 9 — SPATEM — Rebaseline de Estado 2026-10-08"
status: "ACTIVE"
date: "2026-10-08"
project: "ns-3 + Vanetza + SUMO V2X Integration"
stage: "ETAPA 9"
scope: "SPATEM"
baseline: "POST-P21"
baseline_commit: "420db8a"
next_stage: "SPATEM-19 / Fase 9.14 — Robustness"
tags:
  - v2x
  - spatem
  - rebaseline
  - validation
  - acceptance
  - ns3
  - vanetza
  - sumo
  - etapa-9
  - spatem-19
---

# Etapa 9 — SPATEM — Rebaseline de Estado 2026-10-08

## Estado atual

**ETAPA 9:** ACTIVE

**Gates demonstrados:** SPATEM-01 até SPATEM-18

**Próximo gate:** SPATEM-19 — Fase 9.14 — Robustez e validação estrutural

A matriz de "Estado inicial" de [[ETAPA_9_SPATEM_PLANO]] permanece preservada.
Este documento registra o estado efetivamente demonstrado na branch após a
implementação e validação realizadas.

## Matriz atual

| ID | Critério | Estado | Evidência |
|---|---|---|---|
| SPATEM-01 | Post-P21 baseline | PASS | `420db8a` / baseline pós-P21 |
| SPATEM-02 | Vanetza API | PASS | auditoria dos headers e símbolos locais |
| SPATEM-03 | Origin identity | PASS | estação ITS RSU + GN_ADDR explícito |
| SPATEM-04 | Neutral model | PASS | `SpatemInput` / `SpatemMessage` |
| SPATEM-05 | Timing model | PASS | testes de timing/state existentes |
| SPATEM-06 | ASN.1 | PASS | construção SPATEM válida |
| SPATEM-07 | UPER | PASS | encode/decode round-trip |
| SPATEM-08 | BTP | PASS | port 2004 / payload verificados |
| SPATEM-09 | GeoNetworking | PASS | transporte verificado |
| SPATEM-10 | TX | PASS | runtime TX + `PacketContext` |
| SPATEM-11 | RX | PASS | runtime RX até delivery |
| SPATEM-12 | Security | PASS / bounded | fronteira de security explicitamente delimitada |
| SPATEM-13 | Replay | PASS | rejeição de duplicado/replay |
| SPATEM-14 | PacketContext | PASS | continuidade de contexto |
| SPATEM-15 | Logging | PASS | taxonomia SPATEM preservada |
| SPATEM-16 | Analyzer | PASS | analyzer acceptance |
| SPATEM-17 | Synthetic E2E | PASS | infraestrutura -> receiver |
| SPATEM-18 | SUMO TLS | PASS | SUMO TLS -> TraCI -> SumoAdapter |

## Endpoint de infraestrutura

O originador SPATEM é uma estação ITS dedicada com papel RSU.

O endpoint não é um veículo, não é registrado no `VehicleRegistry`, não participa
do `CamScheduler` e não depende de `VehicleState`.

A camada de acesso suporta `ItsStationState`, mantendo o construtor legado
baseado em `VehicleState` para o caminho CAM existente.

## SUMO

O fluxo demonstrado é:

```text
SUMO traffic light
    ->
TraCI
    ->
SumoTrafficLightSpatemMapper
    ->
SumoTrafficLightSpatemIntersectionBuilder
    ->
SPATEM domain model
```

A integração SUMO não altera o caminho comum de transporte.

## Próximo gate — SPATEM-19

A Fase 9.14 deverá validar separadamente:

```text
decode failure
semantic validation failure
replay rejection
routing failure
delivery rejection
```

Os casos mínimos incluem referência de interseção inválida, revisão inválida,
grupo de sinais inválido, timing inválido, estado obrigatório ausente, fase não
suportada, payload malformado, message ID incorreto, dispatch BTP incorreto,
duplicado físico, replay, sender desconhecido e self-origin.

## Limites da evidência

O estado acima demonstra o comportamento dentro da integração ns-3 + Vanetza +
SUMO.

Não constitui demonstração de interoperabilidade externa com equipamento ITS
real ou outro stack externo.

## Relações

[[ETAPA_9_SPATEM_PLANO]]

[[CURRENT_STATE]]

[[DOCUMENT_STATUS]]

[[ETAPA_9_SPATEM_REBASELINE_2026-10-08]]
