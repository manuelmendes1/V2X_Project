---
title: "Etapa 8.14 — Etapa 8 Closeout"
status: "PASS"
date: "2026-09-22"
project: "ns-3 + Vanetza + SUMO V2X"
stage: "8.14"
type: "closeout"
tags:
  - v2x
  - cam
  - denm
  - validation
  - acceptance
  - closeout
  - ns3
  - vanetza
  - sumo
  - etapa-8
  - etapa-8-14
---

# Etapa 8.14 — Etapa 8 Closeout

**STATUS:** COMPLETED
**DATE:** 2026-09-22
**RESULT:** PASS

## Navegação

- [[CURRENT_STATE]]
- [[DOCUMENT_STATUS]]
- [[ETAPA_8_12_E2E_ACCEPTANCE]]
- [[ETAPA_8_13_CAM_REGRESSION]]
- [[ETAPA_8_14_CLOSEOUT]]
- [[../../v2x_analyzer/README]]

## Objetivo

A Etapa 8.14 consolida o estado funcional, os testes, as evidências runtime, a validação do analyzer, a documentação e o histórico Git das etapas anteriores da Etapa 8.

Nenhuma alteração funcional foi introduzida no closeout.

## Evidência 8.12 — DENM E2E

Acceptance:

[[ETAPA_8_12_E2E_ACCEPTANCE]]

Resultado:

```text
8.12_EXIT=0
RESULTADO 8.12: PASS
ACCEPTANCE_EXIT=0
```

A aceitação demonstrou uma cadeia DENM entre origem e receptor distintos, utilizando BTP port `2002`, GeoNetworking GBC, verificação criptográfica, decode, replay, reception e delivery.

## Evidência 8.13 — CAM Regression

Acceptance:

[[ETAPA_8_13_CAM_REGRESSION]]

Resultado:

```text
8.13_EXIT=0
RESULTADO 8.13: PASS
ACCEPTANCE_EXIT=0
```

A regressão foi executada com `--enableDenm=0` e confirmou CAM TX e RX, identidade `port=2001 / SHB / AID 36 / ITS_G5` e ausência de eventos DENM/GBC.

## Evidência v2x_analyzer

Os testes automatizados do analyzer foram executados sobre a árvore atual.

```text
26 passed in 0.15s
ANALYZER_EXIT=0
ANALYZER_TEST_SUITE=PASS
```

Os testes incluem cobertura da taxonomia TX/RX atual e dos eventos DENM específicos.

## Aceitação consolidada

```text
8.12 acceptance      = PASS
8.13 acceptance      = PASS
v2x_analyzer suite   = PASS
overall closeout     = PASS
```

Critérios consolidados:

- evidência runtime real disponível;
- acceptance formal 8.12 aprovado;
- acceptance formal 8.13 aprovado;
- suite do v2x_analyzer com 26 testes aprovados;
- nenhuma alteração funcional introduzida no closeout;
- documentação interligada por wikilinks;
- working tree limpo antes do closeout.

## Estado da Etapa 8

```text
8.0  -> COMPLETE
8.1  -> COMPLETE
8.2  -> COMPLETE
8.3  -> COMPLETE
8.4  -> COMPLETE
8.5  -> COMPLETE
8.6  -> COMPLETE
8.7  -> COMPLETE
8.8  -> COMPLETE
8.9  -> COMPLETE
8.10 -> COMPLETE
8.11 -> COMPLETE / PASS
8.12 -> COMPLETE / PASS
8.13 -> COMPLETE / PASS
8.14 -> COMPLETE / PASS
```

## Limites da evidência

Esta consolidação não demonstra interoperabilidade externa com outra implementação C-ITS/ITS-G5.

As contagens de TX e RX observadas nos diferentes testes não são interpretadas automaticamente como taxa de entrega ou ausência de perdas rádio.

O closeout consolida o comportamento demonstrado no ambiente ns-3 + Vanetza + SUMO e não substitui uma campanha de interoperabilidade externa.

## Histórico Git relevante

```text
78cd7a4  test(denm): add formal 8.12 end-to-end acceptance
5a8bc43  docs(denm): record formal 8.12 E2E acceptance
4e98cab  test(cam): add formal 8.13 regression acceptance
3ddfed1  docs(cam): record 8.13 acceptance commit
```

## Conclusão

A Etapa 8 encontra-se formalmente fechada com evidência runtime para DENM E2E, regressão CAM, validação do v2x_analyzer e documentação correspondente.

O próximo trabalho deve ser tratado como uma nova etapa de desenvolvimento, sem reabrir os acceptance já aprovados, salvo quando uma alteração posterior afetar diretamente os componentes ou contratos validados nesta etapa.
