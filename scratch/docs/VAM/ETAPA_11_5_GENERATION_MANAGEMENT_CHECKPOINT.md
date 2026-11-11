---
title: "Etapa 11.5 — VBS Generation Management — Checkpoint"
status: "IN_PROGRESS"
phase: "11.5"
created: 2026-11-11
baseline_commit: "44e910ab3067808c716516c7f6e43e3a6665e9fe"
tags:
  - VAM
  - VBS
  - validation
---

# Etapa 11.5 — VBS Generation Management — Checkpoint

## Estado

**IN_PROGRESS — checkpoint de revisão, não encerramento da etapa.**

A implementação introduz um gestor autónomo de decisões de geração individual VAM para o cenário inicial Profile 1 — Pedestrian / ActiveStandalone. O gestor ainda não está ligado ao scheduler da VBS nem ao caminho runtime de produção.

## Implementado neste checkpoint

- Avaliação determinística usando tempo lógico de simulação e rejeição de regressão temporal.
- Elegibilidade baseada nas permissões VRU e no estado VBS.
- Intervalo de geração fornecido pelo contexto DCC, limitado ao intervalo configurado de 100–5 000 ms.
- Geração inicial após ativação e triggers por posição, velocidade, orientação circular, timeout e probabilidade de interceção fornecida por modelo externo.
- Agendamento lógico do Low Frequency Container, incluindo a primeira geração e a periodicidade mínima.
- Supressão de redundância com observação de par e sinais alternativos independentes.
- Transação explícita de geração com confirmação e aborto; o snapshot semântico só avança após confirmação.
- Trigger quando surge uma operação de cluster num VAM individual. Nesta versão, VamClusterOperation continua a ser um marcador vazio: apenas a transição de ausência para presença é detetada.

## Evidência de validação

- Teste focado do gestor: **21 asserts C++ PASS**, com VAM_GENERATION_MANAGER=PASS.
- Suite scratch/test/Red/VAM: **7/7 testes PASS**.
- git diff --check: PASS.
- Os testes compilam e executam em harness isolado. Esta evidência não equivale a validação runtime do serviço VAM completo.

## Trabalho ainda necessário

1. Implementar os triggers de aproximação de novos participantes com MSLaD, MSLoD e MSVD, usando fontes e parâmetros espaciais validados.
2. Substituir o marcador de operação de cluster por semântica explícita de join, leave e breakup; cobrir as transições Passive, leader loss e dissolução.
3. Ligar T_CheckVamGen ao scheduler VBS usando tempo lógico, sem introduzir timers arbitrários no gestor.
4. Alimentar a mitigação de redundância com observações reais de LDM/pares, áreas protegidas, zonas de baixo risco e estado de cluster.
5. Integrar o ciclo de geração com construção semântica VAM e medir T_AssembleVAM (< 50 ms) desde o trigger até à entrega a Networking & Transport.
6. Repetir build limpo de produção e regressões CAM/DENM/SPATEM/CPM após a integração.
7. Criar evidência runtime da ativação e geração VBS em cenário pedestrian; não confundir deteção SUMO de um peão com prova de uma ITS-S equipada.

## Limites explícitos

- Nenhum codec ASN.1/UPER VAM foi implementado neste checkpoint.
- Nenhuma assinatura ITS, verificação PKI, BTP, GeoNetworking ou transmissão/receção PHY foi demonstrada.
- O gestor não cria nem agenda callbacks: a execução periódica de T_CheckVamGen pertence ao scheduler que ainda será integrado.
- Não existe medição integrada do orçamento T_AssembleVAM.
- O trigger de probabilidade de interceção só funciona quando o modelo externo fornece uma probabilidade validada; não é calculada nem inventada pelo gestor.
- Não foram adicionados targets VAM ao build de produção nem alterado scratch/CMakeLists.txt.

## Critério para manter a etapa aberta

A Etapa 11.5 permanece **IN_PROGRESS** até os triggers normativos em falta, a semântica de operações de cluster, a integração do scheduler, as entradas reais de redundância e a validação temporal/runtime estarem implementados e demonstrados.
