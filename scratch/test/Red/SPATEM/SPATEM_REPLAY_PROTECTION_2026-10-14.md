---
title: "Proteção contra replay SPATEM — validação unitária"
date: "2026-10-14"
status: "PASS — contrato unitário"
tags:
  - V2X
  - SPATEM
  - replay
  - testing
related_plan: "[[ETAPA_9_SPATEM_PLANO]]"
related_test: "[[test_spatem_replay_protection]]"
---

# Proteção contra replay SPATEM

## Objetivo

Verificar o contrato unitário de deteção de replay para SPATEM, incluindo
mensagens com timestamp semântico e mensagens sem timestamp semântico.

## Resultados

- Timestamp semântico e fingerprint idênticos: rejeitados.
- Timestamp semântico mais antigo: rejeitado sem mutação parcial do estado.
- Domínios temporais distintos: histórico independente.
- Mensagem multi-interseção com uma interseção repetida: rejeição atómica.
- Mesmo payload sem timestamp semântico, repetido após 100 us: rejeitado.
- Mesmo payload sem timestamp semântico, repetido após 1001 us: aceite.

Resultado do teste: 6/6 PASS.

## Limites

A janela de 1000 us é supressão temporária de duplicados. Não prova
proteção geral contra replay, especialmente fora da janela ou quando
não existe timestamp semântico na mensagem. Também não demonstra
a pipeline RX ponta a ponta.

O teste de replay isolado e a suite oficial passaram. O build do executável
de produção terminou com código 0.
