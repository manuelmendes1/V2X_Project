---
title: "SPATEM — teste de fronteira do builder SUMO"
status: "PASS"
date: "2026-10-13"
tags:
  - V2X
  - SPATEM
  - SUMO
  - robustness
  - testing
related_plan: "[[STAPEM]]"
related_builder_test: "[[SPATEM_ASN1_BUILDER_NEGATIVE_2026-10-13]]"
---

# SPATEM — fronteira do builder de interseção SUMO

## Objetivo

Exercitar o `SumoTrafficLightSpatemIntersectionBuilder::Update()` real,
verificando o limite de representação entre `linkIndex` (`uint32_t`) e
`signalGroup` (`uint8_t`).

## Execução

`python3 scratch/test/run_tests.py --test Red/SPATEM/test_sumo_spatem_intersection_builder_boundary.py`

## Resultados observados

| Caso | Resultado |
|---|---|
| `linkIndex = 7` | Aceite |
| `linkIndex = 256` | Rejeitado |
| Estado anterior após rejeição | Preservado |

**Resultado:** 2 casos aprovados, 0 falhas.

Fontes do teste:

- `scratch/test/Red/SPATEM/test_sumo_spatem_intersection_builder_boundary.cc`
- `scratch/test/Red/SPATEM/test_sumo_spatem_intersection_builder_boundary.py`

## Limites

Este teste comprova apenas os comportamentos descritos nesta fronteira.
Não demonstra receção SPATEM E2E por outra estação nem interoperabilidade.

Próximo gate: continuar a matriz de robustez em [[STAPEM]].
