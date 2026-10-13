---
title: "Testes SPATEM da RSU"
status: "ACTIVE"
tags: [V2X, SPATEM, RSU, testing]
related_plan: "[[STAPEM]]"
related_evidence: "[[E2E_RSU_SUMO_TLS_2026-10-13]]"
---

# SPATEM RSU — testes estruturais

Executar todos os testes:

`python3 scratch/test/run_tests.py --suite all`

A suite verifica o despacho para o runtime, a resolução de identidade RSU,
a instalação de mobilidade antes do Wi-Fi e a consolidação de updates TLS
por lote de polling.

Estes testes são estruturais. Logs, configuração SUMO e resumo do ensaio
estão em `evidence/2026-10-13/`. Ver [[E2E_RSU_SUMO_TLS_2026-10-13]]
e [[STAPEM]].

O resultado E2E demonstra transmissão local no cenário testado, não receção
funcional por uma estação remota.
