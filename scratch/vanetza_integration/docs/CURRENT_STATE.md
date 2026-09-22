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
8.13 = TO CONSOLIDATE
8.14 = PENDING
```

Relacionado:

[[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
[[scratch/v2x_analyzer/README]]
