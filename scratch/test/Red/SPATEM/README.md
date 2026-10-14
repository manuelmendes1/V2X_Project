---
title: "Testes negativos SPATEM"
status: "ACTIVE"
date: "2026-10-13"
tags:
  - V2X
  - SPATEM
  - robustness
  - testing
related_plan: "[[STAPEM]]"
related_test: "[[SPATEM_ASN1_BUILDER_NEGATIVE_2026-10-13]]"
---

# Red — SPATEM

Esta pasta contém testes que verificam a rejeição de entradas ou mensagens
inválidas. Um teste aprovado significa que o comportamento negativo esperado
foi observado; não significa que a implementação rejeitou indevidamente
a mensagem.

## Testes negativos disponíveis

Os quatro testes negativos foram executados em 2026-10-14.

| Script | Cobertura | Resultado |
|---|---|---|
| `test_spatem_asn1_builder_negative.py` | Validação semântica e limites ASN.1 | 27/27 PASS |
| `test_spatem_codec_malformed.py` | Rejeição de payload vazio e UPER truncado pelo codec | 2/2 PASS |
| `test_spatem_uper_decoder_malformed.py` | Rejeição de payload vazio e truncado pelo decoder UPER | 2/2 PASS |
| `test_sumo_spatem_intersection_builder_boundary.py` | Limites do índice SUMO e preservação do estado | 2/2 PASS |

Total: 33/33 casos negativos aprovados.

A suite oficial completa, executada com
`python3 scratch/test/run_tests.py --suite all`, terminou com
10 testes executados, zero falhas e zero erros.

Executar a suite negativa:

`python3 scratch/test/run_tests.py --suite red`

Executar um único teste:

`python3 scratch/test/run_tests.py --test Red/SPATEM/test_spatem_asn1_builder_negative.py`

Evidência:
[[SPATEM_ASN1_BUILDER_NEGATIVE_2026-10-13]].
[[SUMO_SPATEM_INTERSECTION_BUILDER_BOUNDARY_2026-10-13]].
## Cobertura ainda pendente

Os testes do codec e do decoder UPER rejeitam payloads vazios e truncados,
mas não demonstram a pipeline RX ponta a ponta. Permanecem por demonstrar,
entre outros, message ID incorreto, dispatch BTP incorreto, duplicados
físicos, proteção contra replay, emissor desconhecido, comportamento
self-origin e entrega funcional a uma estação remota.

A suite executada também não demonstra regressões específicas de CAM ou
DENM. A regressão CAM/DENM continua um gate separado.
## Atualização — Fase 9.14 (2026-10-13)

O teste `test_spatem_asn1_builder_negative.py` contém agora 27 verificações
C++: revisão fora do domínio, limite válido e preservação de duas
interseções distintas, além dos casos anteriores.

O teste complementar
`test_sumo_spatem_intersection_builder_boundary.py` exercita
`SumoTrafficLightSpatemIntersectionBuilder::Update()` e verifica a aceitação
de `linkIndex=7`, a rejeição de `linkIndex=256` e a preservação do estado
anterior após a rejeição. Resultado observado: 2/2 casos aprovados.

A validação continua parcial; não assinalar a Fase 9.14 como concluída.

## Evidência complementar

O teste de fronteira SUMO → SPATEM está documentado em
[[SUMO_SPATEM_INTERSECTION_BUILDER_BOUNDARY_2026-10-13]].
O teste do builder contabiliza atualmente 27 casos aprovados. A Fase 9.14
continua ativa.
