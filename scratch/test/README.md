---
title: "Testes da integração V2X"
status: "ACTIVE"
tags:
  - ns-3
  - V2X
  - testing
related_architecture: "[[ARCHITECTURE]]"
related_plan: "[[STAPEM]]"
---

# Testes da integração V2X

Estrutura oficial para as suites de aceitação positivas (Green) e
negativas (Red) de CAM, DENM e SPATEM.

Executar todas as suites:

`python3 scratch/test/run_tests.py --suite all`

Executar uma suite:

`python3 scratch/test/run_tests.py --suite green`

`python3 scratch/test/run_tests.py --suite red`

Os testes estruturais não substituem a validação funcional, a execução
integrada ou a receção por uma estação remota.

Plano SPATEM: [[STAPEM]].
Arquitetura: [[ARCHITECTURE]].
