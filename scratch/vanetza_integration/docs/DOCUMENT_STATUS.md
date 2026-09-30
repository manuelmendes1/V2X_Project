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

## 8.12 — DENM E2E Acceptance

**STATUS:** COMPLETED  
**DATE:** 2026-09-22  
**RESULT:** PASS

A aceitação formal E2E da fase 8.12 foi concluída com uma execução runtime real.

Evidência:

[[scratch/vanetza_integration/docs/ETAPA_8_12_E2E_ACCEPTANCE]]

Teste:

[[scratch/vanetza_integration/tests/test_denm_e2e_acceptance.sh]]

Commit:

`78cd7a4`

Resultado:

```text
RESULTADO 8.12: PASS
ACCEPTANCE_EXIT=0
```

O caso aceite demonstrou uma cadeia DENM `origin_station=2` → `receiver_station=1`, com `action_seq=1`, passando por BTP-B port 2002, GeoNetworking GBC, TX completion, RX, verificação criptográfica, decode, replay, reception e delivery.

Relacionado:

[[scratch/vanetza_integration/docs/CURRENT_STATE]]

[[scratch/v2x_analyzer/README]]

## 8.13 — CAM Regression Acceptance

**STATUS:** COMPLETED
**DATE:** 2026-09-22
**RESULT:** PASS

A aceitação formal da regressão CAM da fase 8.13 foi concluída com uma execução runtime real e com DENM explicitamente desativado.

Evidência:

[[scratch/vanetza_integration/docs/ETAPA_8_13_CAM_REGRESSION]]

Teste:

[[scratch/vanetza_integration/tests/test_cam_regression_acceptance.sh]]

Commit:

`4e98cab`

Resultado:

```text
RESULTADO 8.13: PASS
ACCEPTANCE_EXIT=0
```

A execução foi realizada com `--enableDenm=0` e terminou com `RUN_EXIT=0`. O pipeline CAM TX apresentou 75 ocorrências em cada estágio desde BTP até `TX_COMPLETE`. O pipeline CAM RX apresentou 133 ocorrências em cada estágio desde `RAW_RX` até `DELIVER_OK`.

A identidade CAM foi preservada com BTP port `2001`, transport `SHB`, ITS-AID `36 / CA` e perfil `ITS_G5`. No RX, o BTP confirmou igualmente port `2001`.

A instrumentação Access RX utiliza `ACCESS_RX_BEGIN` e `ACCESS_RX_OK`, ambos com 133 ocorrências.

Não foram observados eventos DENM/GBC durante a regressão: `GBC_REQUEST`, `DENM_RX_DISPATCH`, `DENM_DECODE_OK`, `DENM_REPLAY_OK`, `DENM_RECEPTION_OK`, `DENM_DELIVER_OK` e `DENM_SELF_ORIGIN_DROP` permaneceram em zero.

As contagens TX=75 e RX=133 não são comparadas entre si como taxa de entrega; a aceitação verifica a integridade das cadeias instrumentadas e a ausência de atividade DENM.

Relacionado:

[[scratch/vanetza_integration/docs/CURRENT_STATE]]

[[scratch/vanetza_integration/docs/ETAPA_8_12_E2E_ACCEPTANCE]]

[[scratch/v2x_analyzer/README]]

Próxima etapa:

[[scratch/vanetza_integration/docs/ETAPA_8_14_CLOSEOUT]]

## 8.14 — Etapa 8 Closeout

**STATUS:** COMPLETED
**DATE:** 2026-09-22
**RESULT:** PASS

O closeout formal da Etapa 8 foi concluído após a reaplicação dos acceptance 8.12 e 8.13 e a execução da suite completa do `v2x_analyzer`.

Evidência:

[[scratch/vanetza_integration/docs/ETAPA_8_14_CLOSEOUT]]

Acceptance 8.12:

[[scratch/vanetza_integration/docs/ETAPA_8_12_E2E_ACCEPTANCE]]

Acceptance 8.13:

[[scratch/vanetza_integration/docs/ETAPA_8_13_CAM_REGRESSION]]

Analyzer:

[[scratch/v2x_analyzer/README]]

Resultado:

```text
8.12_EXIT=0
8.13_EXIT=0
26 passed
ANALYZER_EXIT=0
ETAPA_8_BASELINE_ACCEPTANCE=PASS
```

Próxima fase: desenvolvimento posterior à Etapa 8, sem reabrir os acceptance já aprovados sem uma alteração que afete diretamente o comportamento validado.


## P15 — Certificate Temporal Validity

**STATUS:** VALIDATED  
**DATE:** 2026-09-30  
**RESULT:** PASS

Evidência:

[[scratch/vanetza_integration/docs/P15_Certificate_Temporal_Validity_Validation]]

O P15 foi fechado como validação da validade temporal dos certificados V3. O commit de closeout é d13f005.

## P16 — High-Density DENM Provisioning

**STATUS:** VALIDATED  
**DATE:** 2026-09-30  
**RESULT:** PASS

Evidência:

[[scratch/vanetza_integration/docs/P16_High_Density_DENM_Provisioning]]

A validação foi executada no repositório PKI externo vanetza-pki-lab, com 100 veículos, 3 credenciais por veículo e 300 pares certificado/chave, incluindo verificação de idempotência e exclusão de veh100.

Commit da evidência no repositório PKI:

08eff6ae

Escopo ainda não coberto:

[[scratch/vanetza_integration/docs/P17_P4_Bridge_Integration]]
