---
title: "SPATEM — teste negativo do builder ASN.1"
status: "PASS — cobertura parcial"
date: "2026-10-13"
tags:
  - V2X
  - SPATEM
  - ASN1
  - UPER
  - robustness
  - testing
related_plan: "[[STAPEM]]"
related_green_test: "[[E2E_RSU_SUMO_TLS_2026-10-13]]"
---

# SPATEM — teste negativo do builder ASN.1

## Objetivo

Verificar o comportamento real de `SpatemAsn1Builder::Build()` perante
entradas semanticamente inválidas, assim como confirmar que alguns valores
válidos na fronteira do domínio continuam aceites.

O teste chama o builder de produção. Não é apenas uma procura textual
no código-fonte.

## Ficheiros e execução

- Teste C++: `scratch/test/Red/SPATEM/test_spatem_asn1_builder_negative.cc`
- Invólucro Python: `scratch/test/Red/SPATEM/test_spatem_asn1_builder_negative.py`
- Executor: `scratch/test/run_tests.py`

Comando:

`python3 scratch/test/run_tests.py --test Red/SPATEM/test_spatem_asn1_builder_negative.py`

O invólucro compila o teste C++ num diretório temporário e liga o código
`spatem_asn1_builder.cc` às bibliotecas instaladas. O teste não adiciona
um novo alvo ao CMake de produção e não altera o executável `v2x_sumo`.

## Resultados observados

**Resultado histórico da execução inicial: 22 casos passaram; zero falhas.**

| Caso | Resultado esperado |
|---|---|
| Entrada mínima válida | Aceitar |
| Lista de interseções vazia | Rejeitar |
| `intersectionId = 0` | Rejeitar |
| Lista de movimentos vazia | Rejeitar |
| Bit reservado no estado da interseção | Rejeitar |
| `minuteOfYear = 527040` | Rejeitar |
| `dSecond = 61000` | Rejeitar |
| `dSecond = 65534` | Rejeitar |
| Lista de eventos de movimento vazia | Rejeitar |
| Estado de fase igual a 10 | Rejeitar |
| `minEndTime = 36002` | Rejeitar |
| `maxEndTime < minEndTime` | Rejeitar |
| `likelyTime < minEndTime` | Rejeitar |
| `confidence = 16` | Rejeitar |
| `startTime = 36002` | Rejeitar |
| `nextTime = 36002` | Rejeitar |
| 33 interseções | Rejeitar |
| 256 movimentos | Rejeitar |
| Campos temporais no limite superior suportado | Aceitar |
| `minuteOfYear = 527039` | Aceitar |
| `dSecond = 60999` | Aceitar |
| `dSecond = 65535` (indisponível) | Aceitar |

A última linha representa o valor sentinela de indisponibilidade definido
pelo modelo, não um valor temporal normal.

## Interpretação

O resultado comprova o comportamento do builder para estes inputs concretos.
Não demonstra que todas as entradas inválidas de SPATEM são rejeitadas nem
que o codec ou pipeline RX tenham a mesma cobertura.

Este invólucro Python expõe um único caso `unittest`; os 22 casos são
executados internamente pelo binário C++. Por isso, o relatório regista
**22 verificações C++ na execução inicial**, não 22 casos descobertos pelo `unittest`.

## Limitações e próximos testes

Ainda devem ser testados, conforme aplicável:

- Mais de 16 eventos num movimento.
- `maxEndTime` fora do domínio permitido.
- `likelyTime` superior a `maxEndTime`.
- Payload UPER malformado e descodificação incompleta.
- `messageID` e `protocolVersion` incorretos.
- Dispatch BTP na porta errada e ITS-AID inválido.
- Identidade sem correspondência, emissor desconhecido e origem própria.
- Duplicação física, fingerprint repetido e timestamp antigo no replay.
- Separação entre falha de decode, validação semântica, replay e entrega.

A verificação de `signalGroup > 255` merece revisão de tipo: o campo de
entrada é `uint8_t`, pelo que esse valor não pode ser representado pelo
modelo C++ atual.

## Estado do gate

Resultado: **PASS para a cobertura inicial do builder**.

`SPATEM-19 / Fase 9.14 — Robustness` permanece ACTIVE até a restante matriz
de robustez relevante ter testes e evidências próprios.

Plano de referência: [[STAPEM]].
## Atualização — revisão e fronteira SUMO (2026-10-13)

A contagem inicial de 22 verificações foi ampliada para **27 verificações
aprovadas**.

| Caso adicional | Resultado |
|---|---|
| `revision_above_msg_count_maximum` (`128`) | Rejeitado |
| `revision_maximum_valid_value` (`127`) | Aceite |
| `two_distinct_intersections_preserved` | Duas interseções preservadas |

O caso `revision=128` falhou antes da correção, confirmando a lacuna. Após
a validação explícita do domínio `0..127` antes da atribuição a `MsgCount_t`,
o teste passou.

Foi também removida a condição inalcançável `input.signalGroup > 255` de
`BuildMovement()`, uma vez que `signalGroup` é `uint8_t`. A validação de
valores não representáveis pertence à fronteira SUMO, onde o campo original
`linkIndex` é `uint32_t`.

O teste complementar
`[[SUMO_SPATEM_INTERSECTION_BUILDER_BOUNDARY_2026-10-13]]` executa o builder SUMO
real e passou 2/2 casos: aceita índice 7, rejeita índice 256 e confirma que
a rejeição não modifica o estado previamente construído.

A evidência comprova apenas os casos aqui descritos. A matriz da Fase 9.14
continua incompleta.
## Atualização — cobertura temporal (2026-10-13)

Foram adicionados dois casos negativos ao teste do builder:

- `max_end_time_out_of_range`: `maxEndTime=36002`, rejeitado.
- `likely_time_above_maximum`: `likelyTime=201` com `maxEndTime=200`,
  rejeitado.

Resultado observado: **27 verificações C++, 27 PASS, 0 FAIL**. O teste
complementar da fronteira SUMO passou 2/2 casos; ver
[[SUMO_SPATEM_INTERSECTION_BUILDER_BOUNDARY_2026-10-13]].

A Fase 9.14 mantém-se ativa.
