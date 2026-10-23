---
title: "Etapa 10.5 — Gestão de geração e inclusão de objetos CPM"
status: "ACTIVE"
type: "FUNCTIONAL DEVELOPMENT AND VALIDATION REPORT"
date: 2026-10-23
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
stage: "ETAPA 10.5"
scope: "CPM generation management and object inclusion policy"
baseline_commit: "713a8e9c50f6f46243e208d0870f65c77fdb5a26"
normative_profile: "ETSI TS 103 324 V2.1.1 (2023-06), Release 2"
---

# Etapa 10.5 — Gestão de geração e inclusão de objetos CPM

**Estado:** ACTIVE — política determinística limitada, compilada e testada isoladamente; integração com o codec, assembly CPM, transmissão e interoperabilidade não demonstradas.

**Ramo:** `feature/cpm-etapa10-5-generation-management`

**Baseline:** `713a8e9c50f6f46243e208d0870f65c77fdb5a26` — integração da fonte de perceção, coordenadas e tempo.

## 1. Objetivo

Implementar um componente independente de gestão da geração CPM e seleção de objetos, sem dependência do codec UPER ou da camada de rede.

O serviço recebe candidatos e um instante lógico em milissegundos, decide se existe um evento de geração, avalia as regras de inclusão configuradas e produz um conjunto ordenado de IDs para uma eventual mensagem.

A saída identifica explicitamente se o intervalo ainda não decorreu, se ocorreu um evento sem seleção de objetos ou se foi produzido um conjunto candidato. Nenhum destes resultados constitui prova de encoding, transmissão ou receção CPM.

## 2. Artefactos

- `scratch/vanetza_integration/cpm/perception/cpm_generation_manager.h`
- `scratch/vanetza_integration/cpm/perception/cpm_generation_manager.cc`
- `scratch/test/Green/CPM/test_cpm_generation_manager.cc`
- `scratch/test/Green/CPM/test_cpm_generation_manager.py`
- `scratch/CMakeLists.txt`

O novo ficheiro C++ está incluído em `vanetza_cpm_perception_obj`. O controlador SUMO e o fluxo TX/RX não foram ligados ao gestor nesta fase.

## 3. Referência normativa

Perfil escolhido: ETSI TS 103 324 V2.1.1 (2023-06), Release 2.

Referência oficial: https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf

As cláusulas relevantes são 6.1.2.1 (eventos de geração), 6.1.2.3 (gestão de inclusão de objetos), 6.1.3 (assembly CPM), 7.1.8.6 (qualidade de perceção) e Anexo F (parâmetros de configuração).

O gestor implementa um subconjunto determinístico das regras de inclusão. Não declara conformidade integral ao serviço CPM nem implementa todos os modos normativos de assembly.

## 4. Parâmetros implementados

| Parâmetro | Predefinição | Semântica |
|---|---:|---|
| `generationIntervalMs` | 100 ms | Intervalo mínimo entre eventos avaliados |
| `maximumGenerationIntervalMs` | 1 000 ms | Limite usado na atualização máxima Type-B e na regra de grupo Type-A |
| `objectPerceptionQualityThreshold` | 3 | Um candidato só é elegível se a qualidade for estritamente superior ao limiar |
| `minPositionChangeThresholdMeters` | 4 m | Alteração horizontal de posição para Type-B |
| `minGroundSpeedChangeThresholdMetersPerSecond` | 0,5 m/s | Alteração de velocidade para Type-B |
| `minGroundVelocityOrientationChangeThresholdDegrees` | 4° | Alteração angular mínima para Type-B |
| `maxObjectsPerMessage` | 8 | Limite local de cardinalidade |
| `maxEstimatedPayloadBytes` | 1 200 bytes | Orçamento local estimado |
| `estimatedMessageOverheadBytes` | 64 bytes | Sobrecarga estimada |
| `objectRetentionMs` | 2 000 ms | Expiração local de estado de objetos ausentes |

Os valores normativos de referência incluem T_GenCpmMin = 100 ms para o perfil de base, T_GenCpmMax = 1 000 ms, limiar de qualidade 3, 4 m, 0,5 m/s e 4°. A especificação refere ainda casos de ITS-S rodoviária em que pode ser necessário um intervalo de geração inferior a 100 ms. O gestor atual adota o limite mínimo de 100 ms como decisão do perfil implementado; a exceção de 50 ms não está implementada.

A retenção de 2 000 ms, a cardinalidade 8, a sobrecarga fixa e o orçamento estimado de payload são decisões locais deste MVP, não valores de conformidade normativa.

## 5. Qualidade e classificação

O gestor valida `objectPerceptionQuality` no domínio 0..15. O valor predefinido do limiar é 3 e a regra é estrita: `quality > threshold`.

A qualidade não é calculada pelo gestor. Tem de ser fornecida pelo chamador e pode estar indisponível. Um candidato com qualidade ausente ou igual/inferior ao limiar não é selecionado.

Type-A e Type-B também são fornecidos pelo chamador. A norma define estas categorias a partir da alternativa `ObjectClass` com maior confiança de classificação; o gestor não deriva essa classificação porque a fonte atual não fornece toda a informação necessária para o fazer de forma fundamentada.

## 6. Regras de inclusão

### 6.1 Type-A

O gestor seleciona um Type-A elegível quando foi detetado depois do último evento de geração.

Também implementa atualização de grupo: se existir pelo menos um Type-A elegível cujo tempo desde a última inclusão confirmada atinja `maximumGenerationIntervalMs / 2`, os restantes Type-A elegíveis são considerados para inclusão no mesmo evento.

Os limites locais de cardinalidade e de payload estimado continuam a aplicar-se. Por isso, nem todos os objetos Type-A elegíveis são necessariamente selecionados quando o conjunto excede esses limites.

### 6.2 Type-B

Um Type-B elegível pode ser selecionado quando foi detetado depois do último evento, quando a posição horizontal mudou mais do que o limiar, quando a velocidade mudou mais do que o limiar, quando a orientação mudou pelo menos o limiar, ou quando o tempo desde a última inclusão confirmada atingiu `maximumGenerationIntervalMs`.

A diferença de orientação considera a passagem por 0°/360°.

### 6.3 Primeira inclusão elegível

`FirstEligibleInclusion` é uma razão local usada quando um objeto já observado, mas sem inclusão anterior, passa a ter qualidade elegível. Evita que a sua primeira inclusão fique bloqueada apenas porque a deteção precedeu o evento atual.

Esta extensão do MVP não é apresentada como critério normativo independente. Deve continuar identificada como política local até a integração validar o comportamento completo do serviço.

## 7. Determinismo, estado e diagnóstico

A entrada é validada antes da atualização do estado. IDs nulos ou duplicados, tempo negativo/regressivo, valores não finitos, velocidade negativa, orientação fora de [0°, 360°) e configuração inválida são rejeitados.

A seleção é ordenada por `objectId`, e o resultado distingue:

- `IntervalNotElapsed`;
- `EventDueNoEligibleObjects`;
- `MessageCandidateSetProduced`.

Cada candidato recebe uma razão de decisão, incluindo primeira deteção, alteração de estado, atualização máxima, qualidade insuficiente, cardinalidade máxima ou orçamento estimado excedido.

A ordenação por ID é uma escolha determinística local, não uma implementação da ordenação normativa por Value of Information. Pode diferir da política de prioridade adequada a um cenário real, e não demonstra ausência de omissão por limites de capacidade.

O estado `lastIncludedAtMs` não é atualizado durante a seleção. `Evaluate()` cria uma transação pendente que conserva uma cópia do estado e dos candidatos selecionados. O chamador deve executar `ConfirmMessageCreated(transactionId, error)` depois de confirmar a criação válida da mensagem; só então o gestor regista o instante e os valores da inclusão. `AbortMessageCreation(transactionId, error)` descarta a avaliação pendente sem marcar os objetos como incluídos. O instante da avaliação abortada continua aceite para impor monotonicidade temporal; os tracks provisórios e o instante do último evento de geração não são confirmados. Enquanto existir uma transação pendente, novas avaliações são rejeitadas sem alterar o resultado fornecido ou o estado confirmado. Esta confirmação representa a criação válida da mensagem, não prova encoding UPER, transmissão ou receção física. A API ainda não está integrada no caminho real de assembly/encoding.

## 8. Limites de tamanho

A seleção contabiliza `estimatedPayloadBytes` de cada candidato e uma sobrecarga configurada. Este cálculo é um orçamento aproximado para limitar cardinalidade e crescimento do conjunto.

Não calcula o tamanho ASN.1/UPER real, não verifica `MTU_CPM`, não executa segmentação e não substitui a validação de tamanho após encoding. O valor estimado não pode ser usado como evidência do tamanho efetivamente transmitido.

## 9. Evidência de testes

O contrato C++ independente é compilado com:

`g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror`

As 28 verificações internas do teste do gestor passaram na última execução focada. Foram verificados:

- primeiro objeto e intervalo entre eventos;
- atualização máxima Type-B;
- limiares de posição, velocidade e orientação;
- passagem angular por 360°;
- atualização de grupo Type-A;
- fronteiras de qualidade 0..15 e comparação estrita;
- diferença entre objetos conhecidos e objetos selecionados;
- limites de cardinalidade e seleção determinística;
- orçamento de payload estimado;
- expiração e redeteção;
- preservação da saída em caso de erro e monotonicidade temporal após aborto de transação;
- intervalos e IDs inválidos;
- resultados explícitos de evento sem mensagem e intervalo não decorrido.

### Regressão e build

Resultados obtidos após as alterações ao diagnóstico:

| Verificação | Resultado |
|---|---|
| Contrato transacional do gestor | PASS — seleção, token, confirmação, bloqueio de avaliação pendente e aborto testados |
| Suite `python3 scratch/test/run_tests.py --suite all` | PASS — 58 testes, 0 falhas, 0 erros |
| Build limpo de `ns3.47-v2x_sumo-default` | PASS — ligação do executável concluída |
| `git diff --check` | PASS |
| Teste focado `test_cpm_generation_manager.py` | PASS — `CPM_GENERATION_MANAGER=PASS` |

Estes resultados validam o gestor isolado e a ausência de regressões na suite executada. Não provam o seu comportamento integrado no controlador, nem encoding/transmissão CPM.

## 10. Critérios de saída e limitações

| Critério da Etapa 10.5 | Estado |
|---|---|
| Gestor determinístico independente do codec e da rede | PASS |
| Primeiro objeto, mudanças de estado e atualização máxima | PASS no contrato limitado |
| Ausência de candidatos elegíveis | PASS no contrato |
| Seleção sob limites de cardinalidade e orçamento estimado | PASS |
| Diagnóstico de geração e razões de seleção/omissão | PASS no contrato |
| Classificação Type-A/Type-B derivada de classificação semântica real | PENDENTE |
| Priorização por Value of Information / assembly normativo | FORA DO SUBCONJUNTO |
| Tamanho UPER real e respeito por MTU | PENDENTE para integração posterior |
| API para confirmação explícita da criação de mensagem | PASS — contrato e ciclo transacional testados; chamada pelo assembly real ainda não integrada |
| Integração com controlador SUMO, codec, BTP ou GeoNetworking | FORA DO ÂMBITO desta implementação isolada |
| CPM TX/RX, security, replay, analyzer e interoperabilidade | NÃO DEMONSTRADOS |

## 11. Decisão

A implementação constitui um primeiro gestor determinístico e testado para regras de geração/inclusão CPM, com limites locais explícitos e confirmação transacional do estado de inclusão. O estado permanece `ACTIVE` porque a classificação é fornecida externamente, os limites de tamanho são estimativas e a API transacional ainda não está ligada ao caminho real de assembly/encoding CPM.

A etapa não declara conformidade ETSI integral, encoding adicional, transmissão, receção, security, replay nem interoperabilidade.

## 12. Próximo passo

Integrar `Evaluate()` e o ciclo `ConfirmMessageCreated()`/`AbortMessageCreation()` no caminho real de assembly CPM, confirmando apenas após a criação válida da mensagem. Validar também o tamanho UPER real e manter as regressões CAM, DENM e SPATEM.
