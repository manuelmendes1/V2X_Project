---
title: "PacketIdTag — correlação TX/RX no ns-3"
date: "2026-10-14"
status: "PASS — integração ns-3 controlada"
tags:
  - V2X
  - CAM
  - PacketIdTag
  - TX
  - RX
  - evidence
---

# PacketIdTag — correlação TX/RX

## Objetivo

Validar a propagação do identificador interno do pacote desde
`Ns3AccessAdapter::request()` até `VanetzaRuntime::ReceivePacket()`,
através da stack UDP/IP do ns-3.

## Alterações

- `scratch/vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.cc`
  - Anexa `PacketIdTag` ao pacote ns-3 antes do envio.

- `scratch/vanetza_integration/vanetza_runtime/vanetza_runtime.cc`
  - Recupera a tag na receção, antes de converter o pacote em bytes.
  - Utiliza um identificador de fallback quando não existe uma tag válida.

O identificador permanece metadado interno do simulador; não é acrescentado
ao conteúdo ASN.1 CAM/DENM.

## Cenário

- Executável: `build/scratch/ns3.47-v2x_sumo-default`
- Duração configurada: 20 segundos.
- Dois veículos controlados através de datagramas UDP enviados para
  `127.0.0.1:7400`.
- O segundo veículo foi introduzido após uma fase inicial de 2,5 segundos.
- O cenário utiliza a bridge UDP do projeto; não executa um processo SUMO real.

## Resultados

| Métrica | Resultado |
|---|---:|
| Eventos `TX_PACKET_ID_TAG` | 34 |
| Eventos `RX_PACKET_ID_TAG` | 24 |
| Eventos `RX_PACKET_ID_TAG_MISSING` | 0 |
| Indicações BTP | 24 |
| Identificadores TX/RX comuns | 24 |
| Código de saída da simulação | 0 |

Todos os identificadores RX observados estavam presentes no conjunto de IDs TX.
Não foram observadas receções sem uma tag válida.

Dez IDs TX não apareceram no conjunto de receções desta execução. A causa
não foi determinada; este resultado não deve ser interpretado como uma
medição de entrega completa.

## Outras verificações

- Build de `ns3.47-v2x_sumo-default`: PASS.
- Suite oficial `scratch/test/run_tests.py --suite all`: 12 testes,
  0 falhas, 0 erros.
- `git diff --check`: PASS.

## Log

`test-results/packet_id_tag_staggered_20261014-115006/ns3.log`

## Limites

Esta evidência valida a recuperação da tag num cenário de integração
controlado do ns-3. Não demonstra interoperabilidade externa nem substitui
uma execução E2E com SUMO real.
