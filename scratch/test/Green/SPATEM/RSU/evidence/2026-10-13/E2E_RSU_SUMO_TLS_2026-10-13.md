---
title: "SPATEM RSU — SUMO TLS E2E"
date: "2026-10-13"
status: "PASS — transmissão local"
tags:
  - V2X
  - SPATEM
  - RSU
  - SUMO
  - evidence
related_plan: "[[STAPEM]]"
---

# SPATEM RSU — SUMO TLS E2E

## Resultado

**Transmissão local validada no cenário isolado, sem veículos.**

O cenário utiliza o semáforo SUMO `0`, com 12 links controlados. A bridge
publicou 360 registos TLS.
O controlador consolidou 30 snapshots; movimentos observados:
12.

| Evento TX | Ocorrências |
|---|---:|
| `SPATEM_GENERATE_OK` | 30 |
| `SPATEM_ENCODE_OK` | 30 |
| `BTP_TX_OK` | 30 |
| `GEONET_TX_OK` | 30 |
| `ACCESS_TX_OK` | 30 |
| `SEND_OK` | 30 |
| `TX_COMPLETE` | 30 |

Também foram verificados `CAM_ENCODE_FAILURES=0`, ausência de `senderMobility`
e ausência de `NS_FATAL` no log ns-3.

## Comandos de validação

Build: `cmake --build cmake-cache --target ns3.47-v2x_sumo-default -j2`
  
Código de saída do build: **0**

Testes: `python3 scratch/test/run_tests.py --suite all`
  
Código de saída dos testes: **0**

## Ficheiros de evidência

- `ns3_spatem_rsu_tls_only.log`
- `bridge16_tls_only.log`
- `rilsa3_tls_only.sumocfg`
- `build.log`
- `structural_tests.log`
- `SHA256SUMS.txt`

## Limites

Este ensaio demonstra a fonte e a transmissão SPATEM local, não a receção RX
funcional por outro nó nem interoperabilidade externa. O runtime usa assinatura
de teste; a PKI de produção não foi validada. As coordenadas configuradas são
placeholders, não coordenadas confirmadas do cruzamento do mapa.

Plano de referência: [[STAPEM]]. O próximo gate continua a ser SPATEM-19 /
Fase 9.14 — Robustness.
