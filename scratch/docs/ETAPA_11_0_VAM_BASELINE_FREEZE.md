---
title: "Etapa 11.0 — VAM — Baseline Freeze e Decisão de Aceitação"
project: "V2X_Project"
stage: "ETAPA_11.0"
status: "COMPLETED"
date: "2026-11-04"
baseline_commit: "dd24ba9539f676b7c31e3ab21630115f9ad99a0d"
branch: "feature/vam-etapa11"
previous_stage: "ETAPA 10 — CPM"
next_stage: "ETAPA 11.1 — Auditoria ETSI, Vanetza, ASN.1 e CDD"
---

# Etapa 11.0 — Baseline Freeze e Decisão de Aceitação

## 1. Objetivo

Esta fase congela documentalmente a baseline de entrada para o desenvolvimento VAM.

O objetivo é registar a decisão de aceitação dos resultados de referência dos serviços CAM, DENM, SPATEM e CPM, bem como do build de produção e do analisador V2X.

A decisão permite iniciar a Etapa 11.1 — auditoria das especificações ETSI, API Vanetza, módulos ASN.1/CDD, codec, segurança e transporte VAM.

## 2. Baseline

| Elemento | Referência |
|---|---|
| Repositório | `manuelmendes1/V2X_Project` |
| Branch de trabalho no freeze | `feature/vam-etapa11` |
| Commit-base | `dd24ba9539f676b7c31e3ab21630115f9ad99a0d` |
| Referência remota no freeze | `origin/main` |
| Estado inicial da worktree de freeze | Limpo |
| Data do freeze | 2026-11-04 |

O commit-base corresponde à integração da PR #19, que incorporou o closeout documental da Etapa 10.

Este documento regista a decisão de aceitação da baseline. Não altera retroativamente os relatórios técnicos das etapas anteriores nem substitui os respetivos logs e artefactos de validação.

## 3. Decisão de aceitação

**Decisão: Etapa 11.0 COMPLETED.**

Os seis gates seguintes ficam aceites como PASS para efeitos de entrada na Etapa 11.1:

| Gate | Resultado aceite |
|---|---|
| Baseline CAM | PASS |
| Baseline DENM | PASS |
| Baseline SPATEM | PASS |
| Baseline CPM | PASS |
| Build do executável de produção | PASS |
| Suite do `v2x_analyzer` | PASS |

A aceitação desta matriz constitui a decisão de baseline do responsável pelo projeto. Os resultados são aceites no âmbito definido para esta transição de etapa.

Não se exige que todos os ensaios históricos sejam repetidos exclusivamente para produzir este registo documental.

## 4. Referências de evidência

Os seguintes documentos e artefactos preservam o detalhe técnico da implementação e das validações anteriores.

| Área | Referência |
|---|---|
| Arquitetura comum | `scratch/ARCHITECTURE.md` |
| CPM — closeout | `scratch/docs/CPM/ETAPA_10_CLOSEOUT.md` |
| CPM — segurança e replay | `scratch/docs/CPM/ETAPA_10_10_SEGURANCA_REPLAY.md` |
| CPM — receção e entrega | `scratch/docs/CPM/ETAPA_10_9_RECECAO_ENTREGA.md` |
| CPM — regressão CAM/DENM/SPATEM | `scratch/docs/CPM/ETAPA_10_16_REGRESSAO_RUNTIME_CAM_DENM_SPATEM.md` |
| SPATEM — plano e evidência da etapa | `scratch/docs/SPATEM/ETAPA_9_SPATEM_PLANO.txt` |
| CPM — suites Green e Red | `scratch/test/Green/CPM/` e `scratch/test/Red/CPM/` |
| CPM — pipeline RX focado | Teste `test_cpm_rx_pipeline.py` em `scratch/test/` |
| Analyzer | Suite de testes do `v2x_analyzer`, validada na baseline anterior |

### 4.1 CPM

A evidência de regressão CPM inclui:

- Suite Green: 18 testes, sem falhas.
- Suite Red: 38 testes, sem falhas.
- Total Green + Red: 56 testes, sem falhas.
- Teste focado do pipeline RX: 40 verificações, sem falhas.
- Ensaio E2E sintético com PKI: assinatura, verificação criptográfica e receção CPM no AID 639.
- Ensaio de codificação UPER e limites de cardinalidade/tamanho.

O ensaio E2E sintético com PKI é uma evidência do caminho sintético ns-3. Não deve ser confundido com uma campanha de perceção CPM integralmente alimentada por SUMO.

### 4.2 Build de produção

O target de produção é:

`ns3.47-v2x_sumo-default`

A evidência aceite para esta baseline inclui a validação incremental do target, que terminou sem trabalho adicional e com código de saída zero.

Este registo classifica o build como PASS. Não afirma que foi executado um clean build integral nesta worktree durante o freeze de 2026-11-04.

### 4.3 Analyzer

A suite do `v2x_analyzer` fica aceite como PASS com base na execução de testes anteriormente registada.

A aceitação deste gate não significa que todos os logs históricos tenham sido novamente analisados durante a criação deste documento.

## 5. Interpretação da aceitação

A matriz da secção 3 é a decisão de aceitação da baseline para a transição à Etapa 11.1.

Devem manter-se as seguintes distinções:

- A aceitação de um gate não altera os resultados históricos documentados para uma execução específica.
- Os limites de cenário, cobertura e segurança descritos nos relatórios técnicos continuam a fazer parte do histórico de evidência.
- PASS nesta baseline não equivale a certificação ETSI, conformidade integral ou interoperabilidade OTA formal.
- A aceitação dos serviços individualmente não constitui, por si só, prova de todas as combinações possíveis de execução simultânea.
- Novas alterações devem acrescentar evidência associada ao commit correspondente, preservando os resultados anteriores.

Estas distinções não reabrem os gates aceites. Definem apenas o alcance técnico das afirmações que podem ser feitas sobre a baseline.

## 6. Integridade da worktree e âmbito do freeze

A baseline de referência deste freeze é o commit `dd24ba9539f676b7c31e3ab21630115f9ad99a0d`, integrado em `origin/main`.

Este commit documental contém exclusivamente o registo da decisão de aceitação. Não incorpora alterações de código, testes ou artefactos locais que não pertençam à baseline congelada.

Não fazem parte do commit documental:

- alterações a código de produção;
- alterações aos testes SPATEM ainda não integrados;
- ficheiros ou diretórios não versionados de `sumo/`;
- alterações ao relatório `scratch/docs/CPM/ETAPA_10_CLOSEOUT.md`.

## 7. Decisão de transição

**Etapa 11.0: COMPLETED.**

**Etapa 11.1: AUTORIZADA.**

A Etapa 11.1 pode iniciar a auditoria técnica de VAM, incluindo:

1. Especificações ETSI aplicáveis e versão normativa.
2. API e implementação VAM/VBS disponíveis na instalação Vanetza.
3. Módulos ASN.1 e versão CDD efetivamente utilizados.
4. Tipos, containers, PDU e codificação UPER.
5. ITS-AID, SSP, assinatura, verificação e requisitos de PKI.
6. BTP Type B, porta 2018 e GeoNetworking SHB.
7. Bibliotecas, símbolos, dependências e origem dos artefactos instalados.

A Etapa 11.1 deve identificar a implementação efetivamente disponível no ambiente local antes de introduzir código funcional VAM.

A arquitetura comum do projeto deve ser preservada: identidade, `PacketContext`, runtime Vanetza, segurança, transporte, pipeline RX, replay, logging e analyzer. Não deve ser criado um pipeline paralelo específico de VAM.

A restrição existente em `scratch/CMakeLists.txt` mantém-se: o target de produção continua limitado a `scratch/v2x/v2x_sumo.cc`. Probes e diagnósticos VAM devem ser compilados fora desse target.

## 8. Referências

- [Arquitetura do projeto](../ARCHITECTURE.md)
- [Closeout da Etapa 10 — CPM](CPM/ETAPA_10_CLOSEOUT.md)
- Plano de implementação VAM da Etapa 11, disponibilizado como documento de planeamento da etapa.
