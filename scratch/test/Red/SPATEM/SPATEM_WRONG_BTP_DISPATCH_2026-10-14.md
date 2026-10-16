---
title: "SPATEM — rejeição de porta BTP incorreta"
status: "PASS"
date: "2026-10-14"
tags:
  - V2X
  - SPATEM
  - BTP
  - robustness
  - testing
related_plan: "[[STAPEM]]"
related_message_id_test: "[[SPATEM_WRONG_MESSAGE_ID_2026-10-14]]"
---

# SPATEM — rejeição de porta BTP incorreta

## Objetivo

Demonstrar que `SpatemRxPipeline::Process()` rejeita uma indicação
SPATEM quando `PacketContext::btpDestinationPort` não é a porta SPATEM
esperada (`2004`).

## Método

Teste C++:
`test_spatem_rx_wrong_btp_dispatch.cc`.

Harness Python:
`test_spatem_rx_wrong_btp_dispatch.py`.

O teste constrói uma mensagem SPATEM válida, codifica-a em UPER e
confirma que o codec a descodifica antes de a entregar ao pipeline.
Depois define a porta de destino como `2001` e chama diretamente
`SpatemRxPipeline::Process()`.

## Resultado observado

- Resultado da pipeline: `delivered=0`.
- Etapa final: `BTP_RX`.
- Resultado: `FAIL`.
- Motivo: `invalid_spatem_btp_port`.
- Payload anterior preservado.
- Tipo do payload anterior preservado como `RAW`.
- Mensagem de saída preservada.
- Estado de replay inalterado: zero entradas.
- Caso composto do teste: PASS; código de saída 0.

O pipeline rejeitou a mensagem na validação da porta, antes das etapas
de verificação ITS-AID, descodificação, replay e entrega.

## Regressão

A suite oficial `python3 scratch/test/run_tests.py --suite all`
foi executada depois da adição do teste.

- 12 testes executados.
- Zero falhas e zero erros.
- Duração registada: 15,166 segundos.
- Log: `/tmp/v2x_test_suite_wrong_btp_dispatch_20261014.log`.
- `git diff --check`: código 0.

## Limites

O teste invoca diretamente `SpatemRxPipeline::Process()` com a porta
já presente no contexto. Não demonstra o dispatch BTP ponta a ponta,
a chegada do pacote através de uma rede, nem a entrega por um nó remoto.

Duplicados físicos, replay, emissor desconhecido, self-origin e entrega
remota continuam por demonstrar. A regressão CAM/DENM é um gate separado.

## Estado

Critério wrong BTP dispatch ao nível da pipeline RX: PASS.

SPATEM-19 / Fase 9.14: ACTIVE até os restantes critérios serem demonstrados.

Plano: [[STAPEM]].
