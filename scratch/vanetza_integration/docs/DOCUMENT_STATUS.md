---
status: current
type: documentation-control
date: 2026-09-22
project: ns-3-vanetza-sumo
tags:
  - ns3
  - vanetza
  - v2x
  - denm
  - analyzer
  - documentation
  - validation
  - current
---

# Documentation Status Index

**STATUS:** CURRENT
**TYPE:** DOCUMENTATION CONTROL
**DATE:** 2026-09-22

## CURRENT

Authoritative current-state documents:

- [[scratch/vanetza_integration/docs/CURRENT_STATE]]
- [[scratch/vanetza_integration/docs/vanetza_architecture]]
- [[scratch/v2x_analyzer/README]]

## HISTORICAL

Previous states and snapshots:

- `FASE_0_CONGELAMENTO.md`
- `historical/`
- `historical/project_snapshots/`

Historical documents are retained for traceability only and must not be interpreted as the operational baseline.

## PROCESS

Documents describing development, testing, verification and maintenance procedures.

## PLAN

Documents describing future work, proposed architecture or roadmap items.

## Rule

A statement about the current implementation is authoritative only when it appears in a document marked `CURRENT` or is verified directly against the active source/build.

## P3 — v2x_analyzer / DENM taxonomy

**STATUS:** COMPLETED
**DATE:** 2026-09-22

The analyzer is validated against the current TX/RX taxonomy with DENM events normalized into the common pipeline metrics.

- [[scratch/v2x_analyzer/README]]
- [[scratch/vanetza_integration/docs/CURRENT_STATE]]
- [[scratch/vanetza_integration/docs/vanetza_architecture]]
- [[scratch/vanetza_integration/docs/historical/project_snapshots/code_analyzer]]

Implementation commit: `658c53c713fadda05be3e817e3147461e9b0b463`

Validation: `26/26` analyzer tests, real-log `RESULTADO P3: PASS`, analyzer exit code `0`.
