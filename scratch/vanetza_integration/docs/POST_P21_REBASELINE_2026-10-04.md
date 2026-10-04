---
title: "Post-P21 Rebaseline — Estado Atual do Projeto"
status: "VALIDATED"
type: "TECHNICAL REBASELINE"
date: 2026-10-04
stage: "POST-P21"
domain: "ns-3 / Vanetza / SUMO V2X"
tags:
  - ns3
  - vanetza
  - sumo
  - v2x
  - denm
  - cam
  - pki
  - validation
  - rebaseline
related_state:
  - "[[CURRENT_STATE]]"
related_status:
  - "[[DOCUMENT_STATUS]]"
previous_closeout:
  - "[[P21.7_P21_Closeout]]"
next_stage:
  - "UNDEFINED — requires explicit technical requirement or identified gap"
---

# Post-P21 Rebaseline — Estado Atual do Projeto

## 1. Rebaseline result

O rebaseline parte do estado validado imediatamente após o closeout do P21.

```text
baseline branch:
    docs/post-p20-rebaseline

P21 closeout HEAD:
    7fbd25d

P21 closeout working tree:
    CLEAN

P21:
    CLOSED / VALIDATED

O P21 foi encerrado após validação independente dos acceptance items P21.1–P21.6 e criação do closeout P21.7.
```

## 2. Validation status
P15 = VALIDATED
P16 = VALIDATED
P17 = VALIDATED
P18 = VALIDATED
P19 = VALIDATED
P20 = VALIDATED
P21 = CLOSED / VALIDATED
## 3. P21 acceptance closure
Item	Result
P21.1 — DENM lifecycle	PASS
P21.2 — Analyzer / packetId	PASS
P21.3 — DCC + signing + credential rotation	PASS
P21.4 — High-density + provisioning + bridge	PASS
P21.5 — Identity + credential rotation policy	PASS
P21.6 — Invalid GBC robustness	PASS
P21.7 — Closeout	CLOSED

Todos os acceptance scripts P21.1–P21.6 terminaram com ACCEPTANCE_EXIT=0.

## 4. Historical architecture sequence

As referências antigas P1/P2 não representam a próxima etapa atual.

P1 — VehicleCredential — está documentado como COMPLETED.

P2 — credential pool/discovery — já foi implementado através dos commits históricos:

aaa9cae feat(denm): introduce credential pool and discovery
a8bf814 refactor(security): rename common credential pool
c2348b7 test(denm): align runtime rotation test with credential pool

O próprio material posterior regista P2 como CLOSED.

Consequentemente, links históricos como:

P1 -> P2

não devem ser interpretados como roadmap atual.

## 5. Current architectural state

O estado validado inclui:

VehicleCredential
    -> canonical credential domain model

VehicleCredentialPool
    -> credential collection and active credential state

VehicleSecurityContext
    -> loaded security material and active credential

VehicleCredentialBindingValidator
    -> certificate/private-key ownership validation

DENM V3
    -> signing / verification / lifecycle / rotation

DCC
    -> runtime request / delay / transmission causality

PacketContext
    -> canonical ETSI pipeline observability context

V2X analyzer
    -> TX/RX taxonomy and physical packetId semantics

SUMO / bridge
    -> high-density event ingestion and correlation

GBC TX
    -> validated geodetic-area bounds
## 6. Build and validation infrastructure

The complete ns-3 build succeeded after the P21.6 change:

./ns3 build = PASS

The repository CTest entry point was checked but returned:

No tests were found!!!

Therefore CTest is not considered a P21 acceptance gate for this checkout.

The ns-3 test.py runner was also checked. P21 acceptance programs are standalone scratch executables rather than registered TestSuite entries.

The project therefore uses explicit acceptance scripts for these validation items.

## 7. Validation boundaries

This rebaseline does not claim:

external ETSI interoperability
OTA interoperability
external ITS-station interoperability
formal conformance certification
complete malformed GeoNetworking RX robustness

P21.6 specifically validated TX-side rejection of out-of-range GBC coordinates.

## 8. Current validation frontier

There is currently no repository-backed P22 specification or approved next technical item.

Therefore:

NEXT = UNDEFINED

This is intentional.

The next implementation/validation phase must be selected from an explicit requirement, a demonstrated technical gap, or a formally approved roadmap item.

No historical P1/P2 reference should be promoted automatically to P22.

## 9. Recommended decision gate for the next phase

Before implementation resumes, identify exactly one next technical target and establish:

Problem / requirement
        ↓
Current evidence
        ↓
Expected behavior
        ↓
Focused RED test
        ↓
Minimal implementation
        ↓
GREEN validation
        ↓
Dedicated acceptance
        ↓
Separate commit

The existing project validation rule remains:

plausible code ≠ demonstrated behavior
clean build ≠ functionality validated
local run ≠ interoperability
## 10. Source of truth

The repository Git checkout is authoritative.

Historical up_code_v*.txt material is not authoritative.

The P21 closeout baseline is represented by:

P21 closeout HEAD = 7fbd25d
P21.7 = CLOSED
P21 closeout working tree = CLEAN
## 11. Final status
POST-P21 REBASELINE = VALIDATED

P15 = VALIDATED
P16 = VALIDATED
P17 = VALIDATED
P18 = VALIDATED
P19 = VALIDATED
P20 = VALIDATED
P21 = CLOSED / VALIDATED

NEXT = UNDEFINED
