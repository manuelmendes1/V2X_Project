---
title: "Testes negativos SPATEM"
status: "ACTIVE"
date: "2026-10-14"
tags:
  - V2X
  - SPATEM
  - robustness
  - testing
related_plan: "[[ETAPA_9_SPATEM_PLANO]]"
related_test: "[[SPATEM_ASN1_BUILDER_NEGATIVE_2026-10-13]]"
---

# Red — SPATEM

Esta pasta contém testes que verificam a rejeição de entradas ou mensagens
inválidas. Um teste aprovado significa que o comportamento negativo esperado
foi observado; não significa que a implementação rejeitou indevidamente
a mensagem.

## Testes negativos disponíveis

Os sete scripts iniciais correspondem ao snapshot histórico de 2026-10-14. A tabela foi ampliada com os testes adicionais da Fase 9.14; a suite oficial mais recente é resumida no final deste documento.

| Script | Cobertura | Resultado |
|---|---|---|
| `test_spatem_asn1_builder_negative.py` | Validação semântica e limites ASN.1 | 27/27 PASS |
| `test_spatem_codec_malformed.py` | Payload vazio e UPER truncado no codec | 2/2 PASS |
| `test_spatem_codec_wrong_message_id.py` | Rejeição de messageID MAPEM no codec SPATEM | 2/2 PASS |
| `test_spatem_replay_protection.py` | Replay semântico e supressão temporária de duplicados sem timestamp semântico | 6/6 PASS |
| `test_spatem_rx_wrong_btp_dispatch.py` | Rejeição da porta BTP incorreta no pipeline RX | 1/1 PASS |
| `test_spatem_uper_decoder_malformed.py` | Payload vazio e truncado no decoder UPER | 2/2 PASS |
| `test_sumo_spatem_intersection_builder_boundary.py` | Limites do índice SUMO e preservação do estado | 2/2 PASS |
| `test_spatem_cam_initial_phase_offset.py` | Guarda de regressão do atraso/fase inicial CAM | PASS (suite oficial de 2026-10-16) |
| `test_spatem_multi_intersection_codec_roundtrip.py` | Round-trip do codec com duas interseções | 1/1 PASS |
| `test_spatem_physical_duplicate_hook.py` | Contrato do hook de duplicação física opt-in; ensaio E2E documentado separadamente | PASS (suite oficial de 2026-10-16) |
| `test_spatem_rx_identity_binding.py` | Identidade ausente/incompatível, self-origin e emissor remoto válido | 4/4 PASS |
| `test_spatem_rx_semantic_timing.py` | Validação semântica dos limites temporais | 4/4 PASS |

Total histórico do snapshot de 2026-10-14: 42/42 verificações reportadas. Na execução oficial de 2026-10-16, `python3 scratch/test/run_tests.py --suite all` percorreu 20 ficheiros de teste e executou 34 testes, sem falhas nem erros. O log foi arquivado em `../../Green/SPATEM/RSU/evidence/2026-10-16/official_suite.log`.

Executar a suite negativa:

`python3 scratch/test/run_tests.py --suite red`

Executar o teste BTP isoladamente:

`python3 scratch/test/run_tests.py --test Red/SPATEM/test_spatem_rx_wrong_btp_dispatch.py`

Executar o teste de replay isoladamente:

`python3 scratch/test/run_tests.py --test Red/SPATEM/test_spatem_replay_protection.py`

Evidência:
[[SPATEM_REPLAY_PROTECTION_2026-10-14]]
[[SPATEM_WRONG_BTP_DISPATCH_2026-10-14]]
[[SPATEM_WRONG_MESSAGE_ID_2026-10-14]]
[[SPATEM_ASN1_BUILDER_NEGATIVE_2026-10-13]]
[[SUMO_SPATEM_INTERSECTION_BUILDER_BOUNDARY_2026-10-13]]

## Cobertura pendente no snapshot de 2026-10-14 (histórico)

Os testes `wrong message ID` e `wrong BTP dispatch` validam rejeições
no limite do codec e da pipeline RX, respetivamente. O teste BTP injeta
diretamente a porta no `PacketContext`; não demonstra o encaminhamento
BTP ponta a ponta pela rede.

A proteção contra replay tem cobertura unitária de contrato, não uma
demonstração ponta a ponta da pipeline RX. Sem timestamp semântico, o
teste rejeita fingerprints repetidas dentro de 1000 us e permite a sua
repetição após essa janela; isto não constitui prevenção geral de replay.

Na data deste snapshot, estes quatro critérios permaneciam pendentes. Foram revalidados em 2026-10-16; ver a atualização final deste README e os artefactos em `../../Green/SPATEM/RSU/evidence/2026-10-16/`.

A suite executada também não demonstra regressões específicas de CAM ou
DENM. A regressão CAM/DENM continua um gate separado.

## Atualização — Fase 9.14 (2026-10-13)

O teste `test_spatem_asn1_builder_negative.py` contém agora 27 verificações
C++: revisão fora do domínio, limite válido e preservação de duas
interseções distintas, além dos casos anteriores.

O teste complementar
`test_sumo_spatem_intersection_builder_boundary.py` exercita
`SumoTrafficLightSpatemIntersectionBuilder::Update()` e verifica a aceitação
de `linkIndex=7`, a rejeição de `linkIndex=256` e a preservação do estado
anterior após a rejeição. Resultado observado: 2/2 casos aprovados.

A validação continua parcial; não assinalar a Fase 9.14 como concluída.

## Evidência complementar

O teste de fronteira SUMO → SPATEM está documentado em
[[SUMO_SPATEM_INTERSECTION_BUILDER_BOUNDARY_2026-10-13]].
O teste do builder contabiliza atualmente 27 casos aprovados. A Fase 9.14
continua ativa.

## Atualização — Fase 9.14 (2026-10-16)

### Suite oficial

`python3 scratch/test/run_tests.py --suite all`: 20 ficheiros de teste,
34 testes executados, 0 falhas e 0 erros.

### Evidência integrada — duplicação física e replay

Execução final: `spatem_9_14_duplicate_final_20261016_111246`.

- NS-3 e publicador SUMO/TraCI terminaram com código 0.
- Publicador: 5 040 registos SUMO_TLS e 4 fases observadas.
- Duplicações físicas: 124 identificadores únicos.
- Receções físicas: 248.
- Aceitações pelo replay: 124.
- Rejeições pelo replay: 124.
- Entregas SPATEM: 124.
- `PAIR_MISMATCHES=0`; `PAIR_AUDIT=PASS`.
- Encerramento RSU confirmado por `SHUTDOWN_COMPLETE`.

Por identificador de pacote, a invariável demonstrada foi:
duas receções físicas, uma aceitação, uma rejeição da cópia duplicada e
uma única entrega.

### Evidência integrada — emissor desconhecido

Execução: `spatem_9_14_unknown_sender_current_20261016_110318`.

- NS-3 e publicador terminaram com código 0.
- Publicador: 5 040 registos SUMO_TLS e 4 fases observadas.
- Transmissões SPATEM aceites pela RSU: 117.
- Rejeições `SENDER_NOT_FOUND`: 117.
- Dispatch SPATEM, aceitação/rejeição de replay e entregas: zero.

### Identidade RX e self-origin

O teste `test_spatem_rx_identity_binding.py` passou os quatro casos,
incluindo `self_origin_dropped_before_replay`, com zero entradas de replay,
e aceitou o caso de emissor remoto válido. A suite Green/Red completa
passou nesta data.

### SignalGroupID — domínio e fronteira SUMO

`SignalGroupID` tem domínio ASN.1 INTEGER (0..255), que corresponde ao
tipo `uint8_t` de `SpatemMovement::signalGroup`. Assim, não existe um valor
fora do domínio representável diretamente neste campo; `0` não é tratado
como inválido apenas para forçar um caso negativo.

A fronteira de entrada SUMO é testada separadamente:
`test_sumo_spatem_intersection_builder_boundary.py` aceita `linkIndex=7`,
rejeita `linkIndex=256` e verifica a preservação do estado após a rejeição.
Isto demonstra que o índice de origem não é truncado durante a conversão
para `uint8_t`. A cobertura é de domínio e conversão, não de uma rejeição
direta de um SignalGroupID fora do domínio.

### Limites da conclusão

A evidência integrada corresponde ao ambiente de simulação ns-3/Vanetza
com publicador SUMO/TraCI. Não demonstra interoperabilidade com uma
implementação externa nem validação em hardware ITS-G5.

**Decisão:** SPATEM-19 / Fase 9.14 — matriz de robustez aprovada, pronta
para transição documental à Fase 9.15. A regressão CAM/DENM permanece um
gate separado e ainda não é declarada aprovada.

Artefactos brutos e manifesto SHA-256: `../../Green/SPATEM/RSU/evidence/2026-10-16/`.
