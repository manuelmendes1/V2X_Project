---
title: "P0 — Rebaseline do Checkout Atual"
project: "ns-3 + Vanetza + SUMO V2X"
status: "BASELINE"
type: "AUDIT / RUNTIME BASELINE"
date: "2026-09-25"
head: "87b4903"
branch: "main"
tags:
  - ns3
  - vanetza
  - sumo
  - denm
  - cam
  - baseline
  - audit
  - runtime
related:
  - "[[CURRENT_STATE]]"
  - "[[ETAPA_8_13_CAM_REGRESSION]]"
  - "[[P0_REBASELINE_2026-09-25]]"
---

# P0 — Rebaseline do Checkout Atual

## Estado Git

HEAD atual:

`87b4903 test(denm): add bridge acceptance runner`

Branch:

`main`

O checkout estava limpo durante a auditoria e permanece sem alterações staged, unstaged ou untracked na validação final.

Tags observadas no checkout incluem:

- `etapa-9-baseline-2026-09-22-13-g87b4903`
- `etapa-9-baseline-2026-09-22`
- `etapa-8-9-lifecycle-2026-09-22`
- `etapa-8-8-security-replay-final-2026-09-21`
- `etapa-8-8-security-replay-2026-09-21`
- `etapa-8-cam-baseline-2026-09-18`

## Build

Configuração observada:

- `CMAKE_BUILD_TYPE=default`
- `CMAKE_INSTALL_PREFIX=/usr/local`
- `NS3_ENABLE_TESTS` não definido no `CMakeCache.txt`
- `NS3_ENABLE_EXAMPLES` não definido no `CMakeCache.txt`

Build:

`./ns3 build`

Resultado:

`BUILD_EXIT=0`

## Infraestrutura de testes

O runner raiz:

`python3 test.py --list`

não apresenta testes registados.

Isto é uma característica importante da baseline atual: os testes DENM standalone não estão registados no `test.py`/CTest como suítes ns-3.

Foram encontrados 24 executáveis DENM standalone em `build/`, incluindo testes de:

- credential discovery
- credential pool rotation
- lifecycle
- scheduler
- replay
- SUMO clock domain
- V3 credential rotation
- V3 runtime E2E
- high-density runtime
- multi-vehicle runtime

## Python analyzer

Execução:

`python3 -m pytest scratch/v2x_analyzer/tests/ -q`

Resultado:

`26 passed in 0.08s`

## Bridge acceptance

Runner:

`scratch/vanetza_integration/tests/test_denm_bridge_acceptance.sh`

Resultado:

- `TEST_COUNT=5`
- `FAILURES=0`
- `DENM_BRIDGE_ACCEPTANCE_EXIT=0`

Os cinco testes são independentes do `cwd`.

## Runtime baseline

Executável:

`build/scratch_v2x_sumo`

A execução sem bridge SUMO produziu apenas inicialização/encerramento do adapter, sem veículos e sem pipeline CAM. Portanto, essa execução isolada não constitui uma baseline funcional.

A execução válida foi feita com:

- ns-3 em `127.0.0.1:7400`
- `v2x_bridge_15.py`
- `--enableDenm=0`
- `--simDuration=5`

Resultado:

`P0_NS3_EXIT=0`

Veículos observados:

- `veh0`
- `veh1`
- `veh2`
- `veh3`

Tráfego SUMO → ns-3:

`255` observações

CAM TX:

- `GENERATE_OK=38`
- `ENCODE_OK=38`
- `SIGN_OK=38`
- `BTP_TX_OK=38`
- `GEONET_TX_OK=38`
- `ACCESS_TX_OK=38`
- `SEND_OK=38`
- `TX_COMPLETE=38`

CAM RX:

- `RAW_RX=3`
- `ACCESS_RX_OK=3`
- `GEONET_RX_OK=3`
- `BTP_RX_OK=3`
- `VERIFY_OK=3`
- `DECODE_OK=3`
- `REPLAY_OK=3`
- `RECEPTION_OK=3`
- `DELIVER_OK=3`

Runtime:

`ERROR=0`

## CAM acceptance

O acceptance test:

`scratch/vanetza_integration/tests/test_cam_regression_acceptance.sh`

terminou com:

`RESULTADO 8.13: PASS`

Foram confirmados:

- BTP CAM TX usando port `2001`
- SHB
- AID `36`
- GeoNetworking `ITS_G5`
- CAM RX port `2001`
- veículo observations para `veh0`–`veh3`

## DENM observation

A execução foi feita com:

`--enableDenm=0`

A acceptance CAM confirmou:

`no DENM/GBC events observed (0)`

Contudo, o log ainda contém infraestrutura DENM, incluindo:

- `DENM V3 signer=DUMMY`
- `DENM V3 verifier=REAL`
- `DENM_EVENT_INGEST`
- `DENM_V3_VERIFY_ENTRY`

Essas ocorrências vêm da interação com a bridge SUMO e demonstram que a flag `--enableDenm=0` não significa ausência completa de atividade relacionada com DENM.

Esta observação é registada, mas não é corrigida em P0.

## Baseline operacional congelada

A baseline atual é:

`SUMO bridge → ns-3 VehicleState → CAM generation → Vanetza/GeoNetworking/BTP → ns-3 Wi-Fi → RX pipeline`

com:

- 4 veículos observados
- 38 CAM TX completos
- 3 CAM deliveries
- verificação criptográfica RX observada
- replay protection observada
- acceptance CAM PASS
- analyzer PASS
- bridge acceptance PASS
- build PASS

## Limitações conhecidas

O `test.py --list` não representa a totalidade real dos testes do projeto.

Os testes DENM C++ e bridge Python são atualmente executados por mecanismos standalone/acceptance.

A bridge SUMO reside fora do checkout principal:

`../v2x_sumo/integration/v2x_bridge_15.py`

e é uma dependência operacional do runtime SUMO.

## Relações

[[CURRENT_STATE]]

[[ETAPA_8_13_CAM_REGRESSION]]

[[P0_REBASELINE_2026-09-25]]

O próximo problema da sequência é [[P1_Modelo_Explicito_VehicleCredential]].
