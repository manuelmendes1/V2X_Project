---
status: current
type: architectural-operational-baseline
date: 2026-09-22
project: ns-3-vanetza-sumo
tags:
  - ns3
  - vanetza
  - sumo
  - v2x
  - denm
  - analyzer
  - validation
  - current-state
---

# Current State — ns-3 + Vanetza + SUMO Integration

**STATUS:** CURRENT
**TYPE:** ARCHITECTURAL AND OPERATIONAL BASELINE
**DATE:** 2026-09-22

This document is the authoritative description of the current integration state.
Historical snapshots, previous freezes, exploratory notes and test artefacts are not operational baselines.

## 1. Architecture Boundary

Vanetza-specific integration code is isolated under:

```
scratch/vanetza_integration/wrappers/vanetza/
```

The orchestration layer is:

```
scratch/vanetza_integration/vanetza_runtime/
```

The ns-3/SUMO entry point is:

```
scratch/v2x/v2x_sumo.cc
```

No active `vanetza_link_adapter` exists.

## 2. Identity Contract

The following identifiers are distinct:

```
Link MAC ≠ GN_ADDR.MID ≠ stationId
```

Authority:

```
VehicleIdentity
    → identity authority

VanetzaRuntime / GeoNetworking
    → GN_ADDR authority

VehicleRegistry
    → state and reverse indexes
```

`VehicleRegistry` must not redefine authoritative vehicle identity or GN_ADDR.

## 3. Security Ownership Contract

```
security/
    → operations / algorithms

context/vehicle_security/
    → state / ownership
```

## 4. RX Pipeline

```
RAW_RX
→ ACCESS_RX
→ GEONET_RX
→ BTP_RX
→ VERIFY
→ DECODE
→ REPLAY
→ RECEPTION
→ DELIVER
```

`DELIVER` is the terminal delivery stage.

`RECEPTION` is an intermediate reception stage.

`BEACON` is auxiliary GeoNetworking processing and is not a CAM reception event.

## 5. TX Pipeline

```
GENERATE
→ ENCODE
→ SIGN
→ BTP_TX
→ GEONET_TX
→ ACCESS_TX
→ SEND
→ TX_COMPLETE
```

`TX_COMPLETE` is the terminal successful TX event.

## 6. Document Classification

```
CURRENT
    implemented operational state

HISTORICAL
    previous states, snapshots, freezes and old experiments

PROCESS
    development, testing, verification and maintenance procedures

PLAN
    future work or proposed architecture
```

Only documents explicitly classified as `CURRENT` define the operational baseline.

## 7. Validation Principle

```
plausible code ≠ demonstrated behavior
clean build ≠ validated functionality
local run ≠ external interoperability
```

## 8. Current Analyzer State

The current V2X analyzer is aligned with the operational pipeline taxonomy and has been validated against a real ns-3 + SUMO + Vanetza execution.

### 8.1 TX taxonomy

```text
GENERATE
→ ENCODE
→ SIGN
→ BTP_TX
→ GEONET_TX
→ ACCESS_TX
→ SEND
→ TX_COMPLETE
```

### 8.2 RX taxonomy

```text
RAW_RX
→ ACCESS_RX
→ GEONET_RX
→ BTP_RX
→ VERIFY
→ DECODE
→ REPLAY
→ RECEPTION
→ DELIVER
```

### 8.3 DENM analyzer integration

DENM events are normalized into the current analyzer taxonomy:

```text
DENM/GENERATE_OK       → GENERATE
DENM/ENCODE_OK         → ENCODE

SECURITY/SIGN_BEGIN    → SIGN
SECURITY/SIGN_OK       → SIGN

DENM/DENM_DECODE_OK    → DECODE
DENM/DENM_REPLAY_OK    → REPLAY
DENM/DENM_RECEPTION_OK → RECEPTION
DENM/DENM_DELIVER_OK   → DELIVER
```

The DENM V3 security service emits structured `SIGN_BEGIN` and `SIGN_OK` events while retaining the existing `[DENM_V3_SIGN]` diagnostic marker.

### 8.4 P3 validation evidence

Validated against:

`output_log_etapa_8_10_v2.txt`

Results:

```text
Build                         PASS
Runtime                       PASS
DENM security logging         PASS
Analyzer tests                26/26 PASS
Real-log analyzer             RESULTADO P3: PASS
Analyzer exit code            0
Structured DENM SIGN_OK       4
TX successful stages          106 / 106 / 106 / 106 / 106 / 106 / 106 / 106
RX successful stages          154 / 154 / 154 / 154 / 154 / 154 / 154 / 154 / 154
```

Implementation commit:

`658c53c713fadda05be3e817e3147461e9b0b463`

Commit message:

`fix(v2x-analyzer): align DENM events with current pipeline taxonomy`

### 8.5 Related documentation

- [[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
- [[scratch/vanetza_integration/docs/vanetza_architecture]]
- [[scratch/v2x_analyzer/README]]
- [[scratch/vanetza_integration/docs/historical/project_snapshots/code_analyzer]]

## 9. Etapa 8.12 — Formal E2E Acceptance

A fase 8.12 foi formalmente aceite em 2026-09-22 através de uma execução runtime real.

Resultado:

```text
RESULTADO 8.12: PASS
ACCEPTANCE_EXIT=0
```

Teste formal:

[[scratch/vanetza_integration/tests/test_denm_e2e_acceptance.sh]]

Documento de evidência:

[[scratch/vanetza_integration/docs/ETAPA_8_12_E2E_ACCEPTANCE]]

Commit do teste:

`78cd7a4`

A validação demonstrou uma cadeia DENM completa entre `origin_station=2` e `receiver_station=1`, atravessando BTP port 2002, GeoNetworking GBC, TX completion, RX, verificação, decode, replay, reception e delivery.

Estado atual:

```text
8.11 = COMPLETE
8.12 = COMPLETE / PASS
8.13 = COMPLETE / PASS
8.14 = COMPLETE / PASS
```

Relacionado:

[[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
[[scratch/v2x_analyzer/README]]

### Etapa 8.13 — CAM Regression Acceptance

**STATUS:** COMPLETE / PASS
**DATE:** 2026-09-22

A regressão CAM foi formalmente validada com DENM desativado através de `--enableDenm=0`.

Evidência formal:

[[scratch/vanetza_integration/docs/ETAPA_8_13_CAM_REGRESSION]]

Teste:

[[scratch/vanetza_integration/tests/test_cam_regression_acceptance.sh]]

Resultado:

```text
RUN_EXIT=0
RESULTADO 8.13: PASS
ACCEPTANCE_EXIT=0
```

CAM TX: 75 ocorrências em cada estágio instrumentado até `TX_COMPLETE`.

CAM RX: 133 ocorrências em cada estágio instrumentado desde `RAW_RX` até `DELIVER_OK`.

Identidade CAM: BTP port `2001`, transport `SHB`, ITS-AID `36 / CA`, profile `ITS_G5`.

DENM/GBC: zero eventos observados na execução de regressão.

Esta validação não interpreta TX=75 e RX=133 como taxa de entrega.

Relacionado:

[[scratch/vanetza_integration/docs/ETAPA_8_12_E2E_ACCEPTANCE]]

[[scratch/vanetza_integration/docs/ETAPA_8_13_CAM_REGRESSION]]

[[scratch/v2x_analyzer/README]]

Próxima etapa:

[[scratch/vanetza_integration/docs/ETAPA_8_14_CLOSEOUT]]

### Etapa 8.14 — Closeout

**STATUS:** COMPLETE / PASS
**DATE:** 2026-09-22

O closeout formal da Etapa 8 foi concluído após validação independente das aceitações 8.12 e 8.13 e da suite completa do `v2x_analyzer`.

Evidência:

[[scratch/vanetza_integration/docs/ETAPA_8_14_CLOSEOUT]]

8.12:

[[scratch/vanetza_integration/docs/ETAPA_8_12_E2E_ACCEPTANCE]]

8.13:

[[scratch/vanetza_integration/docs/ETAPA_8_13_CAM_REGRESSION]]

Analyzer:

[[scratch/v2x_analyzer/README]]

Resultado consolidado:

```text
8.12 = PASS
8.13 = PASS
v2x_analyzer = 26 passed
ETAPA 8.14 = PASS
```

## P1 — Modelo Explícito VehicleCredential

[[P1_Modelo_Explicito_VehicleCredential]]


## P16 — High-Density DENM Provisioning

**STATUS:** VALIDATED  
**DATE:** 2026-09-30

O provisioning PKI DENM foi validado independentemente do runtime ns-3/SUMO para 100 veículos com três credenciais por veículo, totalizando 300 pares certificado/chave.

A evidência operacional pertence ao repositório:

~~~
/home/mmendes/vanetza-pki-lab
~~~

Commit de evidência:

~~~
08eff6ae test(p16): validate high-density DENM provisioning
~~~

O resultado valida a capacidade de provisionamento e a idempotência do conjunto. A observação de que a integração ns-3/SUMO ainda não estava validada corresponde ao estado no encerramento do P16. Essa lacuna foi posteriormente fechada pelo P17.

Relacionado:

[[scratch/vanetza_integration/docs/P17_P4_Bridge_Integration]]


## P17 — P4 Bridge / SUMO Integration

[[scratch/vanetza_integration/docs/P17_P4_Bridge_Integration]]

**STATUS:** VALIDATED
**RESULT:** PASS

P17 validou a integração bridge/SUMO/ns-3 por correlação de 7 eventos DENM da bridge com 7 eventos de ingestão no ns-3.

```text
bridge_events=7
ns3_ingest_events=7
matched=7
missing=0
unexpected=0
```

## P18 — DENM Credential Binding Ownership

[[scratch/vanetza_integration/docs/P18_Credential_Binding_Ownership]]

**STATUS:** VALIDATED
**RESULT:** PASS

P18 validou rejeição de credential binding certificado/chave inválido antes da alteração da credencial ativa.

Commit: `6807573`

## P19 — CAM / DENM Regression Acceptance

[[scratch/vanetza_integration/docs/P19_CAM_DENM_Regression_Acceptance]]

**STATUS:** VALIDATED
**RESULT:** PASS

P19 confirmou a regressão integrada CAM + DENM pós-P18.

## P20 — High-Density DENM V3 Runtime Validation

[[scratch/vanetza_integration/docs/P20_High_Density_V3_Runtime_Validation]]

**STATUS:** VALIDATED
**RESULT:** PASS

P20 validou o envelope de 10 e 20 veículos e a extensão de 100 veículos, incluindo rotação de três credenciais.

```text
UNIQUE_GLOBAL_SIGNER_COUNT=300
DENM_HIGH_DENSITY_V3_RUNTIME_ROTATION_EXIT=0
```

## Current validation frontier

```text
P15 = VALIDATED
P16 = VALIDATED
P17 = VALIDATED
P18 = VALIDATED
P19 = VALIDATED
P20 = VALIDATED
P21 = NOT DEFINED
```

Não existe atualmente um P21 formalmente definido no checkout. O próximo item técnico deve resultar de um requisito ou lacuna verificável.

Relacionado:

[[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
