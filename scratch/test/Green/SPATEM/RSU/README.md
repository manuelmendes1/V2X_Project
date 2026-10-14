---
title: "Testes SPATEM RSU"
status: "ACTIVE"
date: "2026-10-13"
tags:
  - V2X
  - SPATEM
  - RSU
  - testing
related_plan: "[[STAPEM]]"
related_evidence: "[[E2E_RSU_SUMO_TLS_2026-10-13]]"
---

# SPATEM RSU — testes e evidência

Esta pasta contém quatro testes estruturais e a evidência do ensaio
integrado SUMO TLS → RSU → transmissão SPATEM.

## Testes estruturais

| Ficheiro | O que verifica | Limite |
|---|---|---|
| `test_spatem_tx_dispatch.py` | Existe um caminho ativo de chamada para `SpatemService::Send()` e o controlador encaminha a transmissão para o runtime RSU. | Não demonstra por si só transmissão em runtime. |
| `test_rsu_ipv4_sender_resolution.py` | O caminho de resolução IPv4 não está limitado à resolução direta para `VehicleState`; procura também uma inicialização RSU ativa. | Não prova receção funcional de um pacote RSU. |
| `test_spatem_rsu_mobility.py` | A RSU instala `ConstantPositionMobilityModel` antes da instalação Wi-Fi. | É um teste estrutural, não um ensaio do canal RF. |
| `test_spatem_poll_batch.py` | O polling SUMO precede o envio do snapshot; os updates marcam o estado como dirty; o envio utiliza a interseção acumulada. | Não comprova, isoladamente, a transmissão dos pacotes. |

Executar a suite SPATEM RSU completa:

`python3 scratch/test/run_tests.py --suite green`

Executar um único teste, por exemplo:

`python3 scratch/test/run_tests.py --test Green/SPATEM/RSU/test_spatem_poll_batch.py`

## Evidência runtime

O ensaio de 13 de outubro de 2026 utilizou o cenário SUMO TLS isolado,
sem veículos, com 12 links controlados. A bridge enviou 360 registos TLS;
o controlador gerou 30 snapshots aceites, cada um com 12 movimentos.
Foram observados 30 eventos `TX_COMPLETE` e zero falhas CAM de codificação
no cenário isolado.

Relatório: [[E2E_RSU_SUMO_TLS_2026-10-13]].

A evidência demonstra transmissão local no cenário ensaiado. Não demonstra
receção E2E por outro nó, interoperabilidade externa ou PKI de produção.

## Robustez

O teste direto do builder encontra-se na suite negativa:
[[SPATEM_ASN1_BUILDER_NEGATIVE_2026-10-13]].

O estado global da Etapa 9 e os gates pendentes estão documentados em
[[STAPEM]].
