---
title: "SPATEM — rejeição de message ID incorreto"
status: "PASS"
date: "2026-10-14"
tags:
  - V2X
  - SPATEM
  - robustness
  - testing
related_plan: "[[STAPEM]]"
related_builder_test: "[[SPATEM_ASN1_BUILDER_NEGATIVE_2026-10-13]]"
related_boundary_test: "[[SUMO_SPATEM_INTERSECTION_BUILDER_BOUNDARY_2026-10-13]]"
---

# SPATEM — rejeição de message ID incorreto

## Objetivo

Demonstrar que `VanetzaSpatemCodec::Decode()` rejeita uma mensagem UPER
cuja estrutura ASN.1 é descodificável, mas cujo `header.messageID` não
corresponde ao identificador SPATEM.

## Método

Teste C++: `test_spatem_codec_wrong_message_id.cc`.

Harness Python: `test_spatem_codec_wrong_message_id.py`.

O teste constrói uma mensagem SPATEM válida, com `stationId=9001`,
uma interseção e um movimento. Executa dois casos:

1. Controlo positivo: codificação UPER e descodificação pelo codec SPATEM.
2. Caso negativo: altera `header.messageID` para
   `ItsPduHeader__messageID_mapem`, codifica através do encoder UPER
   e entrega o payload ao codec SPATEM.

## Resultado observado

- `valid_spatem_control`: PASS; `success=1`, `stationId=9001`.
- `wrong_message_id_rejected`: PASS; `success=0`.
- Erro observado: `invalid SPATEM messageID`.
- Casos: 2/2 aprovados; zero falhas.
- Harness isolado: terminou com código 0.

A mensagem MAPEM não foi rejeitada por falha de descodificação UPER:
o codec produziu especificamente o erro de `messageID` inválido.

## Regressão

A suite oficial `python3 scratch/test/run_tests.py --suite all`
foi executada depois da adição do teste.

- 11 testes executados.
- Zero falhas e zero erros.
- Log: `/tmp/v2x_test_suite_wrong_message_id_20261014.log`.
- `git diff --check`: código 0.

## Limites

Este teste valida o limite do codec. Não demonstra, isoladamente,
o dispatch BTP, a pipeline RX completa, a proteção contra replay,
a resolução de emissores desconhecidos, self-origin ou a entrega
a uma estação remota.

## Estado

Critério `wrong message ID` ao nível do codec: PASS.

SPATEM-19 / Fase 9.14: ACTIVE até os restantes critérios da matriz
serem demonstrados.

Plano: [[STAPEM]].
