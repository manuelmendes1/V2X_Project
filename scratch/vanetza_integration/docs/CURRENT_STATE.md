# Current State — ns-3 + Vanetza + SUMO Integration

**STATUS:** CURRENT
**TYPE:** ARCHITECTURAL AND OPERATIONAL BASELINE
**DATE:** 2026-09-18

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
