---
title: "Etapa 11.5 - VBS Generation Management - Checkpoint"
status: "IN_PROGRESS"
phase: "11.5"
created: 2026-11-11
updated: 2026-11-13
branch: "main"
baseline_commit: "44e910ab3067808c716516c7f6e43e3a6665e9fe"
substep_11_5_2: "IMPLEMENTED / UNIT-VALIDATED"
substep_11_5_3: "IMPLEMENTED / UNIT-VALIDATED / BUILD-VALIDATED"
substep_11_5_4: "IMPLEMENTED / UNIT-VALIDATED / INTEGRATION-TESTED / MERGED"
source_branch: "feature/vam-etapa11-5-4-vehicle-observation-history"
merge_pr: 31
merge_commit: "bf5ad295a4ac5d888ad81bed35a829eddd17f245"
tags:
  - VAM
  - VBS
  - validation
---

# Etapa 11.5 - VBS Generation Management - Checkpoint

## Estado

**IN_PROGRESS — checkpoint de implementação e validação, não encerramento da Fase 11.5.**

O `VamGenerationManager` implementa decisões determinísticas de geração individual VAM para o cenário inicial Profile 1 — Pedestrian / ActiveStandalone. O componente e os restantes módulos VAM abaixo descritos estão agora incluídos no target de objetos `vanetza_vam_obj`, ligado ao executável de produção. Contudo, a avaliação ainda não está ligada a um scheduler VBS de runtime.

**Sub-etapa 11.5.2 — Safe Proximity Trigger: IMPLEMENTED / UNIT-VALIDATED.** O manager suporta o trigger de proximidade com observações e limiares fornecidos externamente. A integração de produção com scheduler e transmissão VAM continua pendente.

**Sub-etapa 11.5.3 — SUMO Person First-Observation History: IMPLEMENTED / UNIT-VALIDATED / BUILD-VALIDATED.** `SumoPersonState` e `SumoPersonRegistry` preservam `firstObservedAtUs` durante cada período ativo de uma pessoa, terminando-o num REMOVE aceite e iniciando um novo período após reentrada válida.

**Sub-etapa 11.5.4 — Vehicle Lifecycle, Proximity Snapshot e Cluster Lifecycle: IMPLEMENTED / UNIT-VALIDATED / INTEGRATION-TESTED / MERGED.** A entrega acrescenta o histórico de presença de veículos SUMO, a construção de snapshots de proximidade para pessoas e veículos, o adaptador de avaliação do manager, operações de cluster semanticamente tipadas e a máquina de estados de lifecycle persistida por participante. Foi integrada através do PR #31 em `main`.

O merge foi realizado em 2026-11-13. O merge commit é `bf5ad295a4ac5d888ad81bed35a829eddd17f245`; o commit de origem da última alteração da branch foi `c7707689b6b43393b22a2eaea4b30cd8d6c64ba1`.

A integração dos componentes no build e a existência de testes do adaptador não demonstram que o scheduler VBS os invoque em runtime nem que uma operação de cluster seja transmitida como VAM.

## Implementado neste checkpoint

### Generation manager e triggers

- Avaliação determinística com tempo lógico de simulação e rejeição de regressão temporal.
- Elegibilidade baseada nas permissões VRU e no estado VBS.
- Intervalo de geração fornecido pelo contexto DCC, limitado ao intervalo configurado de 100–5 000 ms.
- Geração inicial após ativação e triggers por posição, velocidade, orientação, timeout, probabilidade de interceção fornecida por modelo externo e proximidade segura.
- Trigger de proximidade com `firstObservedAtMs`, LaD/MSLaD, LoD/MSLoD e VD/MSVD. As três condições estritas têm de ser verdadeiras em simultâneo, e a primeira observação tem de ser posterior à última geração confirmada.
- Validação de timestamps, distâncias e limiares, incluindo rejeição de valores não finitos e negativos.
- Agendamento lógico do Low Frequency Container e supressão de redundância baseada em observações de pares e sinais alternativos.
- Transação explícita de geração com confirmação e aborto. `ConfirmGeneration` utiliza `confirmedAtMs` como instante lógico da entrega confirmada a Networking & Transport.

### Observação e ciclo de vida SUMO

- `SumoPersonRegistry` preserva o instante da primeira observação aceite de cada pessoa durante o período ativo.
- `SumoVehicleLifecycleRegistry` mantém estado ativo/inativo, instante da primeira observação e timestamp do último evento aceite. Rejeita eventos fora de ordem e só aceita uma reentrada quando existe timestamp que prove que o UPSERT é posterior ao REMOVE.
- `VamProximitySnapshotBuilder` constrói snapshots a partir de `SumoPersonRegistry`, `VehicleRegistry` e `SumoVehicleLifecycleRegistry`.
- Os snapshots propagam os instantes de presença para pessoas e veículos. Veículos inativos, sem histórico temporal recuperável ou sem amostra de mobilidade válida não são introduzidos como targets válidos.
- A categoria SUMO Person não é usada para inferir automaticamente o perfil semântico VRU.
- Os limiares de proximidade são configuração explícita do chamador, não defaults normativos implícitos.

### Avaliação de geração a partir dos snapshots

- `VamProximityProvider` constrói as observações seguras de proximidade e mantém histórico próprio de presença.
- `VamGenerationEvaluationAdapter` liga snapshots SUMO, observações de proximidade e `VamGenerationManager::Evaluate`.
- O adaptador prepara o histórico de proximidade de forma transacional: só o confirma internamente quando a avaliação do manager é aceite.
- A confirmação ou o aborto da geração continua sob responsabilidade da camada que futuramente executar a transmissão. O adaptador não implementa scheduler nem transporte.

### Operações e lifecycle de clusters

- `VamClusterOperation` passou de marcador vazio para estrutura com campos opcionais independentes para join, leave, breakup e mudança de Cluster ID.
- `VamClusterOperationValidation` valida os payloads e respetivos tempos, razões e intervalos.
- `VruClusterLifecycleStateMachine` implementa um reducer determinístico para criação, join, confirmação, cancelamento, falha, leave, perda de leader, breakup, exceção CPM, low-risk e mudança de Cluster ID.
- O contexto de cluster é persistido por participante em `SumoVruSubjectRegistry`.
- A integração impede alterações diretas ao VBS que contornem o lifecycle e mantém a máquina de estados de papel VRU como autoridade sobre o estado VBS.
- `SimulationController` expõe a operação `ApplyVruSubjectClusterLifecycle`, encaminhando eventos para o registry e devolvendo o resultado do reducer.
- O reducer não agenda notificações: os períodos de repetição e os eventos `Complete*` continuam sob responsabilidade de um scheduler externo.
- As operações produzidas pelo reducer ainda não estão ligadas a um emissor de cluster VAM.

### Build

Os seguintes componentes estão declarados em `vanetza_vam_obj`:

- `vam_generation_manager.cc`
- `vam_cluster_operation_validation.cc`
- `vam_cluster_lifecycle_state_machine.cc`
- `vam_proximity_provider.cc`
- `vam_proximity_snapshot_builder.cc`
- `vam_generation_evaluation_adapter.cc`

O target de objetos é incluído na ligação do executável de produção.

## Evidência de validação

### Generation manager e observação

- Teste focado do `VamGenerationManager`: **26 checks C++ PASS**, com `VAM_GENERATION_MANAGER=PASS`.
- Os testes de proximidade cobrem as três condições estritas, a primeira deteção posterior à última transmissão confirmada, o intervalo mínimo e o ciclo abort/retry/confirm.
- A matriz de entradas inválidas cobre 20 observações com timestamps inválidos, distâncias ou limiares negativos, `NaN` e infinito.
- Teste de primeira observação de pessoas SUMO: **16 checks C++ PASS**.
- Suite Python `scratch/test/Red/VAM`: **13/13 ficheiros de teste PASS** após a adição do histórico de primeira observação de pessoas.
- O `compile_commands.json` foi verificado para confirmar a compilação de `sumo_person_registry.cc` no target de produção.

### Vehicle lifecycle e proximity adapter

- Os testes focados do ciclo de vida de veículos cobrem UPSERT, REMOVE, ordenação temporal, reentrada e elegibilidade de dispatch.
- Os testes do proximity snapshot builder cobrem seleção de pessoas e veículos ativos, histórico temporal, amostras inválidas, limiares configurados e ordenação determinística.
- Os testes do `VamGenerationEvaluationAdapter` verificam a construção de observações a partir dos registos SUMO, a avaliação do manager e a preservação transacional do histórico de proximidade.

### Cluster lifecycle

- Build focado do reducer: `VAM_CLUSTER_LIFECYCLE_BUILD=PASS`.
- Testes do reducer: `VAM_CLUSTER_LIFECYCLE_STATE_MACHINE=PASS`.
- Build focado da integração SUMO: `SUMO_VRU_SUBJECT_CLUSTER_LIFECYCLE_BUILD=PASS`.
- Testes da integração SUMO: `SUMO_VRU_SUBJECT_CLUSTER_LIFECYCLE=PASS`.
- A suite VAM completa executada após as alterações finais do lifecycle: **54 testes PASS**, em 26,533 s.
- A cobertura inclui payloads persistidos inválidos, transições join/leave/breakup, confirmação, perda de leader, mudança de Cluster ID, exceção CPM, isolamento por participante e remoção terminal SUMO.

### Build e integração Git

- Build de produção executado com `cmake --build cmake-cache --target ns3.47-v2x_sumo-default -j2`: código de saída 0; o Ninja indicou `no work to do` na última execução.
- A presença da entrada de compilação do lifecycle reducer, o objeto compilado e o comando de link do `vanetza_vam_obj` foram verificados.
- `git diff --cached --check`: PASS antes do commit do lifecycle.
- Commit `c7707689b6b43393b22a2eaea4b30cd8d6c64ba1` criado com 14 ficheiros, 3 586 inserções e 5 remoções.
- PR #31 merged em `main` em 2026-11-13 às 14:20:44 UTC, através do merge commit `bf5ad295a4ac5d888ad81bed35a829eddd17f245`.

**Limite da evidência:** os testes demonstram o comportamento isolado e a integração ao nível dos snapshots, do adapter, do reducer e do registry. Não demonstram scheduler VBS em execução nem transmissão VAM real. O build de produção sem trabalho pendente, por si só, não equivale a um clean rebuild.

## Trabalho ainda necessário

1. Integrar `VamGenerationEvaluationAdapter::EvaluateForEgo` num scheduler VBS de runtime, usando tempo lógico e a política `T_CheckVamGen`. O adaptador está implementado e testado, mas não existe ainda um consumidor runtime que o invoque.
2. Ligar a avaliação aceite à construção semântica do VAM, ao codec ASN.1/UPER, à assinatura e autorização ITS e ao caminho comum BTP/GeoNetworking.
3. Executar a confirmação da geração apenas depois da entrega efetivamente confirmada a Networking & Transport, invocando `ConfirmGeneration` com o timestamp lógico correto; gerir abortos e retries no chamador.
4. Ligar as operações devolvidas pelo lifecycle de clusters a uma representação e transmissão de cluster VAM. Definir a sequência de emissão relativamente às transições de estado, em particular na exceção CPM em que o VBS passa diretamente para Passive.
5. Integrar entradas reais da política de redundância, observações de pares, áreas protegidas e zonas de baixo risco, mantendo a origem e o referencial das observações explicitamente definidos.
6. Validar os thresholds LaD, LoD e VD e os referenciais usados no cenário SUMO, sem os apresentar como defaults normativos nem inferir um perfil VRU apenas a partir da categoria SUMO.
7. Medir `T_AssembleVAM` desde o trigger até à entrega a Networking & Transport e demonstrar que respeita o orçamento de 50 ms.
8. Demonstrar em runtime a ativação e desativação VBS, geração individual e transições de cluster num cenário pedestrian. Uma pessoa SUMO detetada não constitui, por si só, prova de uma ITS-S equipada.
9. Executar um clean rebuild e a regressão CAM/DENM/SPATEM/CPM/analyzer após a integração runtime. Reavaliar as duas expectativas estruturais anteriormente identificadas em `test_spatem_rsu_physical_position.py` e `test_cpm_sumo_step_boundary.py`; não as considerar resolvidas sem nova evidência.
10. Guardar evidência de runtime, logs e métricas e atualizar este checkpoint antes de declarar a Fase 11.5 fechada.

## Limites explícitos

- Não foi demonstrado codec VAM ASN.1/UPER nem transmissão/receção VAM end-to-end.
- Não foi demonstrada assinatura ITS, verificação PKI VAM, autorização por AID/SSP ou interoperabilidade com outra stack.
- `VamGenerationManager` e os módulos auxiliares estão declarados no target de objetos e ligados ao executável de produção, mas o manager ainda não é invocado por um scheduler VBS de runtime.
- `VamGenerationEvaluationAdapter` liga snapshots a `VamGenerationManager::Evaluate` quando chamado; não existe ainda integração que o invoque durante a execução normal da simulação.
- Os snapshots usam os registos SUMO como fonte de estado simulado. Isto não demonstra uma fonte física de perceção nem uma LDM de produção.
- LaD, LoD, VD e os limiares são fornecidos por configuração externa. O pipeline não estabelece por si só que os thresholds representam valores normativos universais.
- O reducer de clusters e o registry persistem estados e devolvem operações semânticas. Não existe ainda consumidor de produção que transforme essas operações em VAM de cluster e as transmita.
- Os eventos de conclusão de períodos de join, leave, breakup e mudança de ID são externos ao reducer; não há scheduler temporal integrado.
- A exceção CPM tem representação de estado própria, mas não foi demonstrada uma transmissão ordenada da operação de breakup através de um caminho VAM/CPM runtime.
- A API `ConfirmGeneration` está implementada e testada, mas não existe um ciclo de transmissão VAM real que a invoque após confirmação de envio.
- Não existe medição integrada do orçamento `T_AssembleVAM`.
- O trigger de probabilidade de interceção depende de um modelo externo validado; não é calculado pelo manager.
- Não há demonstração de interoperabilidade OTA, PC5/ITS-G5 com equipamento real ou certificação ETSI.

## Critério para manter a fase aberta

A Fase 11.5 permanece **IN_PROGRESS**. As sub-etapas 11.5.2, 11.5.3 e 11.5.4 têm componentes implementados e evidência de validação nos âmbitos descritos neste checkpoint, mas isso não satisfaz a definição de DONE da Fase 11.5.

A fase só poderá ser encerrada depois de demonstrados, no âmbito do cenário e perfil declarados:

- integração do scheduler VBS com as fontes de observação e com o `VamGenerationManager`;
- triggers individuais, DCC, low-frequency e mitigação de redundância no caminho runtime;
- construção VAM, codec, segurança, BTP/GeoNetworking e confirmação temporal de transmissão;
- receção, verificação, freshness, plausibilidade e entrega à LDM/aplicação;
- operações de cluster emitidas e recebidas no caminho protocolar apropriado, incluindo join, leave, breakup, perda de leader e mudanças de ID;
- cenário pedestrian end-to-end com evidência runtime;
- build de produção limpo e regressões CAM/DENM/SPATEM/CPM/analyzer PASS;
- documentação e artefactos de evidência completos.

**Estado da sub-etapa 11.5.4:** IMPLEMENTED / UNIT-VALIDATED / INTEGRATION-TESTED / MERGED.

**Estado da Fase 11.5:** IN_PROGRESS — não declarar VAM funcional end-to-end nem encerrar a fase com base apenas nesta entrega.
