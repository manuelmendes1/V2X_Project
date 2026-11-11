---
title: "Etapa 11.5 - VBS Generation Management - Checkpoint"
status: "IN_PROGRESS"
phase: "11.5"
created: 2026-11-11
updated: 2026-11-11
branch: "feature/vam-etapa11-5-2-safe-proximity-trigger"
baseline_commit: "44e910ab3067808c716516c7f6e43e3a6665e9fe"
substep_11_5_2: "IMPLEMENTED / UNIT-VALIDATED"
tags:
  - VAM
  - VBS
  - validation
---

# Etapa 11.5 - VBS Generation Management - Checkpoint

## Estado

**IN_PROGRESS - checkpoint de implementação e validação, não encerramento da fase.**

O `VamGenerationManager` é um componente autónomo de decisões de geração individual VAM para o cenário inicial Profile 1 - Pedestrian / ActiveStandalone. Continua sem ligação ao scheduler da VBS e ao caminho runtime de produção.

**Sub-etapa 11.5.2 - Safe Proximity Trigger: IMPLEMENTED / UNIT-VALIDATED ao nível do manager isolado.** A integração da fonte de perceção/LDM e a demonstração de transmissão VAM real continuam pendentes.

## Implementado neste checkpoint

- Avaliação determinística usando tempo lógico de simulação e rejeição de regressão temporal.
- Elegibilidade baseada nas permissões VRU e no estado VBS.
- Intervalo de geração fornecido pelo contexto DCC, limitado ao intervalo configurado de 100-5 000 ms.
- Geração inicial após ativação e triggers por posição, velocidade, orientação circular, timeout e probabilidade de interceção fornecida por modelo externo.
- Trigger de proximidade segura para novos participantes, baseado em observações externas com:
  - instante lógico da primeira deteção;
  - distância lateral LaD e limiar MSLaD;
  - distância longitudinal LoD e limiar MSLoD;
  - diferença vertical VD e limiar MSVD.
- A condição de proximidade só dispara quando a primeira deteção é estritamente posterior à última transmissão VAM confirmada e as três desigualdades são verdadeiras simultaneamente: `LaD < MSLaD`, `LoD < MSLoD` e `VD < MSVD`. Não é utilizada uma distância euclidiana agregada nem uma condição OR entre eixos.
- Validação das observações de proximidade: timestamp não negativo e não posterior ao instante de avaliação; distâncias e limiares finitos e não negativos.
- O contrato de observação não expõe uma identidade persistente do participante.
- Agendamento lógico do Low Frequency Container, incluindo a primeira geração e a periodicidade mínima.
- Supressão de redundância com observação de par e sinais alternativos independentes.
- Transação explícita de geração com confirmação e aborto; o snapshot semântico só avança após confirmação.
- `ConfirmGeneration` recebe agora `confirmedAtMs`, o instante lógico da entrega confirmada a Networking & Transport. O gestor rejeita confirmações anteriores à avaliação e usa o instante confirmado como baseline de `T_GenVam` e do Low Frequency Container.
- Trigger quando surge uma operação de cluster num VAM individual. Nesta versão, `VamClusterOperation` continua a ser um marcador vazio: apenas a transição de ausência para presença é detetada.

## Evidência de validação

- Teste focado do `VamGenerationManager`: **26 checks C++ PASS**, com `VAM_GENERATION_MANAGER=PASS`.
- Os testes de proximidade cobrem as três condições estritas, a primeira deteção posterior à última transmissão confirmada, o intervalo mínimo, o ciclo abort/retry/confirm e a não repetição do trigger.
- A matriz de entradas inválidas cobre 20 observações com timestamps negativos/futuros, distâncias ou limiares negativos, `NaN` e infinito. A rejeição preserva o objeto de saída e o estado temporal do manager.
- Suite Python `scratch/test/Red/VAM`: **12/12 testes PASS**.
- Build de produção `./ns3 build`: **PASS**, `PRODUCTION_BUILD_EXIT_CODE=0`; o Ninja concluiu a ligação do executável `v2x_sumo`.
- A verificação de `cmake-cache/compile_commands.json` devolveu `MANAGER_COMPILE_COMMANDS=0`. O build de produção não compilou `vam_generation_manager.cc`; portanto, valida o target de produção existente, não a integração do novo manager.
- Regressão transversal: 60 testes atuais identificados. A primeira execução terminou com 57 PASS e 3 FAIL. O CPM Synthetic E2E foi posteriormente repetido com a fixture PKI explícita de 2026-11-03 e passou, elevando a evidência individual para **58/60 PASS**.
- O CPM Synthetic E2E repetido demonstrou `CPM_SYNTHETIC_E2E_SECURITY=PASS`, `CPM_SYNTHETIC_E2E_SEMANTICS=PASS` e entrega ao receiver. Esta evidência pertence ao CPM e não demonstra transmissão VAM.
- Duas falhas estruturais permanecem por resolver, fora do âmbito desta alteração:
  1. `test_spatem_rsu_physical_position.py` procura o builder em `spatem/emitter/its_station_network_builder.h`. O builder atual está em `context/its_station/network/its_station_network_builder.h`.
  2. `test_cpm_sumo_step_boundary.py` procura literalmente `while should_continue_simulation(` e falha ao localizar esse marcador. O bridge atual publica veículos, pessoas e TLS, emite `SUMO_STEP_END` e só depois chama `receive_from_ns3`; os outros dois testes do mesmo ficheiro passaram. A expectativa estrutural do teste necessita de revisão.
- `git diff --check`: PASS.
- Os testes do manager compilam e executam em harness isolado. Esta evidência não equivale à validação runtime do serviço VAM completo.

## Trabalho ainda necessário

1. Ligar `VamSafeProximityObservation` a uma fonte de perceção/LDM validada. A fonte deve determinar a novidade do participante, o referencial das distâncias e os limiares MSLaD, MSLoD e MSVD adequados aos perfis, velocidades e condições aplicáveis.
2. Substituir o marcador de operação de cluster por semântica explícita de join, leave e breakup; cobrir as transições Passive, leader loss e dissolução.
3. Ligar `T_CheckVamGen` ao scheduler VBS usando tempo lógico, sem introduzir timers arbitrários no gestor.
4. Alimentar a mitigação de redundância com observações reais de LDM/pares, áreas protegidas, zonas de baixo risco e estado de cluster.
5. Integrar o ciclo de geração com construção semântica VAM e medir `T_AssembleVAM` (< 50 ms) desde o trigger até à entrega a Networking & Transport.
6. Repetir build limpo de produção e regressões CAM/DENM/SPATEM/CPM após a integração. Corrigir separadamente as duas expectativas estruturais desatualizadas identificadas na regressão atual.
7. Criar evidência runtime da ativação e geração VBS em cenário pedestrian; não confundir deteção SUMO de um peão com prova de uma ITS-S equipada.

## Limites explícitos

- Nenhum codec ASN.1/UPER VAM foi implementado neste checkpoint.
- Nenhuma assinatura ITS, verificação PKI VAM, BTP, GeoNetworking ou transmissão/receção PHY VAM foi demonstrada.
- O `VamGenerationManager` não está presente em `cmake-cache/compile_commands.json` nem ligado ao executável de produção.
- O gestor não cria nem agenda callbacks: a execução periódica de `T_CheckVamGen` pertence ao scheduler que ainda será integrado.
- As observações de proximidade e respetivos limiares são fornecidos externamente. O manager não calcula LaD, LoD, VD, MSLaD, MSLoD ou MSVD, nem valida o referencial físico usado pela fonte.
- A API de confirmação temporal está implementada e testada, mas ainda não existe um consumidor runtime que a invoque após uma transmissão VAM efetiva.
- Não existe medição integrada do orçamento `T_AssembleVAM`.
- O trigger de probabilidade de interceção só funciona quando o modelo externo fornece uma probabilidade validada; não é calculada nem inventada pelo gestor.
- Não foram adicionados targets VAM ao build de produção nem alterado `scratch/CMakeLists.txt`.

## Critério para manter a fase aberta

A Fase 11.5 permanece **IN_PROGRESS** até os triggers e as respetivas fontes reais, a semântica de operações de cluster, a integração do scheduler, as entradas reais de redundância e a validação temporal/runtime estarem implementados e demonstrados.

A sub-etapa 11.5.2 está implementada e validada ao nível unitário do manager. Isso não encerra a integração da condição de proximidade no serviço VAM de produção.
