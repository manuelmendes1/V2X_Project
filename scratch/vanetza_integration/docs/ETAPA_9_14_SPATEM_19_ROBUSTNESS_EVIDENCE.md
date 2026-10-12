---
title: "SPATEM-19 / Fase 9.14 — Robustness Evidence"
date: 2026-10-08
status: "GREEN_WITH_EXPLICIT_LIMITATIONS"
phase: "9.14"
requirement: "SPATEM-19"
branch: "feature/spatem-sumo-9-13"
head: "15ed197"
tags:
  - ns3
  - vanetza
  - v2x
  - spatem
  - etsi
  - robustness
  - validation
  - evidence
  - testing
related:
  - "[[ETAPA_9_SPATEM_PLANO]]"
  - "[[ETAPA_9_SPATEM_REBASELINE_2026-10-08]]"
  - "[[CURRENT_STATE]]"
  - "[[DOCUMENT_STATUS]]"
---

# SPATEM-19 / Fase 9.14 — Robustness Evidence

## Scope

Esta etapa consolida a evidência de robustez do SPATEM-19.

Princípio de aceitação:

> código plausível não é comportamento demonstrado; build limpo não é validação; execução local não é interoperabilidade.

As categorias de rejeição devem permanecer distintas:

`DECODE_FAIL` ≠ `semantic DROP` ≠ `REPLAY_DROP` ≠ `routing rejection` ≠ `DELIVER rejection`.

## Estado global

**RESULTADO: GREEN COM LIMITAÇÕES EXPLICITAMENTE REGISTADAS**

Regressão focada após o binding de identidade:

```text
CODEC=0
RX_PIPELINE=0
UNKNOWN_SENDER=0
WRONG_BTP_PORT=0
REAL_RX=0
REAL_REPLAY=0

Evidência adicional de wrong message ID:

BUILD_RC=0
TEST_RC=0
PASS wrong SPATEM messageID is rejected
```
## Matriz formal

| ID | Caso | Evidência | Ponto | Estado |
|---|---|---|---|---|
| R-01 | Invalid intersection identifier | `spatem_codec_boundary_test` | Encode / semantic builder | GREEN |
| R-02 | Invalid revision / MsgCount | `spatem_codec_boundary_test` | Encode / ASN.1 constraint | GREEN |
| R-03 | Invalid signal group | `signalGroup` é `uint8_t`; `0` é válido | Sem valor inválido representável no contrato | N/A |
| R-04 | Invalid timing | `spatem_codec_boundary_test` | Encode / semantic builder | GREEN |
| R-05 | Missing mandatory movement state/list | listas vazias no codec boundary test | Encode / ASN.1 constraint | GREEN |
| R-06 | Unsupported phase state | `spatem_codec_boundary_test` | Encode / semantic builder | GREEN |
| R-07 | Malformed encoded payload | `test_spatem_real_rx_acceptance.sh` | `DECODE_FAIL` | GREEN |
| R-08 | Wrong message ID | caso 23 do codec boundary test | Codec messageID validation | GREEN |
| R-09 | Wrong BTP dispatch | teste dedicado de porta errada executado e PASS | BTP dispatch rejection | GREEN |
| R-10 | Sender identity mismatch | `spatem_rx_pipeline_test` | `DECODE` semantic validation | GREEN |
| R-11 | Self-origin | `spatem_rx_pipeline_test` | `DECODE` semantic validation | GREEN |
| R-12 | Unknown sender | `spatem_unknown_sender_runtime_test` | Runtime routing/source lookup | GREEN |
| R-13 | Duplicate content / replay | `test_spatem_real_replay_acceptance.sh` | `REPLAY_DROP` | GREEN |
| R-14 | Independent physical duplicate | sem frame identifier físico no modelo | Não demonstrável independentemente | LIMITATION |
| R-15 | Unknown configured intersection reference | sem registry semântico de interseções no RX atual | Não demonstrável independentemente | LIMITATION |

## R-01 — Invalid intersection identifier

O builder rejeita explicitamente:

intersectionId == 0

Evidência:

PASS intersectionId 0 is rejected

Implementação:

scratch/vanetza_integration/wrappers/vanetza/spatem/spatem_asn1_builder.cc

Esta prova demonstra um identificador inválido. Não demonstra uma registry global de interseções configuradas.

## R-02 — Invalid revision / MsgCount

Foi demonstrada a rejeição do limite inválido:

PASS MsgCount value 128 is rejected

A validação está associada ao campo de revisão representado no contrato SPATEM local.

## R-03 — Invalid signal group

SpatemMovement::signalGroup é uint8_t.

Logo, o domínio representável é:

0..255

O valor 0 foi demonstrado como válido no caminho SUMO/SPATEM.

Não existe atualmente um valor >255 representável sem alterar o contrato de tipo.

Portanto este caso é:

N/A

e não deve ser transformado artificialmente numa rejeição inexistente.

## R-04 — Invalid timing

Foram demonstradas as seguintes rejeições:

PASS TimeMark value 36002 is rejected
PASS confidence value 16 is rejected
PASS likelyTime beyond maxEndTime is rejected
PASS likelyTime below minEndTime is rejected
PASS maxEndTime below minEndTime is rejected

Também foi demonstrado o limite válido:

PASS likelyTime equal to minEndTime and maxEndTime is accepted

Foram ainda demonstrados limites válidos de TimeMark e DSecond, incluindo o valor indisponível 65535.

## R-05 — Missing mandatory movement state/list

Foram demonstradas rejeições de:

PASS empty MovementList is rejected
PASS empty MovementEventList is rejected
PASS empty IntersectionStateList is rejected

Estas provas cobrem a ausência de estruturas obrigatórias no contrato de construção/encoding atual.

## R-06 — Unsupported phase state

Evidência:

PASS MovementPhaseState value 10 is rejected
## R-07 — Malformed encoded payload

Execução real:

socket
  -> VanetzaRuntime
  -> BTP port 2004
  -> SPATEM dispatch
  -> DECODE
  -> SPATEM_DECODE_FAIL

Contagens observadas:

SOCKET_RX_COUNT=1
VRT_RX_COUNT=1
SPATEM_2004_DISPATCH_COUNT=1
SPATEM_DECODE_FAIL_COUNT=1
SPATEM_RECEPTION_COUNT=0
SPATEM_DELIVER_COUNT=0
BTP_2004_UNDISPATCHED_COUNT=0

Resultado:

RESULTADO 9.6.6 GREEN: PASS
SPATEM_REAL_RX=PASS

Conclusão: o payload malformado chega efetivamente ao decoder SPATEM e é rejeitado antes de RECEPTION e DELIVER.

## R-08 — Wrong message ID

O teste começa com um SPATEM válido, decodifica a estrutura ASN.1, altera somente:

header.messageID = ItsPduHeader__messageID_cam

Depois reencoda a estrutura e executa:

VanetzaSpatemCodec::Decode()

A rejeição é específica:

invalid SPATEM messageID

Evidência:

PASS wrong SPATEM messageID is rejected
SPATEM_CODEC_BOUNDARY_TEST_EXIT=0
TEST_RC=0

Não houve alteração no codec de produção para produzir esta evidência.

## R-09 — Wrong BTP dispatch

Contrato SPATEM:

BTP port = 2004
ITS-AID  = 137

Evidência real:

dst_port=2004
Handler found=YES
EV=SPATEM_RX_DISPATCH

Na evidência anterior de payload malformado:

BTP_2004_UNDISPATCHED_COUNT=0

Isto demonstra que o BTP 2004 está ligado ao handler SPATEM e não é tratado como undispatched.

### Evidência adicional — teste negativo de BTP port

Teste dedicado: `scratch/spatem_rx_pipeline_test/spatem_rx_pipeline_test.cc`

Resultado: `BUILD_RC=0`, `TEST_RC=0`, `RESULTADO R-09 WRONG BTP PORT: PASS`

Caso executado: `btpDestinationPort=2005`, payload SPATEM válido.

Resultado observado: `STAGE=BTP_RX`, `RESULT=FAIL`, `REASON=invalid_spatem_btp_port`.

O processamento terminou em `BTP_RX`; não houve avanço para `VERIFY`, `DECODE`, `REPLAY`, `RECEPTION` ou `DELIVER`.

A classificação `PipelineResult::FAIL` é a semântica atual de `PacketContext` para esta falha de transporte/validação.

R-09: `GREEN`.

## R-10 — Sender identity mismatch

O RX SPATEM compara a identidade semântica da mensagem:

decoded message.stationId

com a identidade do emissor resolvida pelo runtime:

context.senderStationId

Quando diferentes:

REASON=spatem_sender_identity_mismatch
EV=SPATEM_SENDER_IDENTITY_MISMATCH

Evidência:

payload_station=1001
resolved_sender_station=2001

PASS SPATEM sender identity mismatch is rejected
PASS sender identity mismatch context is marked DROP
PASS sender identity mismatch reason is reported
PASS sender identity mismatch does not reach REPLAY
PASS sender identity mismatch does not reach RECEPTION
PASS sender identity mismatch does not reach DELIVER
## R-11 — Self-origin

O pipeline rejeita explicitamente um SPATEM cujo sender e receiver representam a mesma station/node.

Evidência:

EV=SPATEM_SELF_ORIGIN_DROP
REASON=self_reception

Com:

PASS self-originated SPATEM is rejected
PASS self-originated SPATEM context is marked DROP
PASS self-originated SPATEM reason is self_reception
PASS self-originated SPATEM does not reach RECEPTION
PASS self-originated SPATEM does not reach DELIVER

A rejeição ocorre depois de DECODE_OK, antes de REPLAY.

## R-12 — Unknown sender

O VanetzaRuntime resolve o emissor pela origem IPv4.

Quando o endereço não está no VehicleRegistry, o runtime agora retorna de forma controlada:

[VRT_RX] SENDER_NOT_FOUND source_ip=10.99.99.99

Evidência:

PASS unknown source IPv4 is absent from VehicleRegistry
PASS ReceivePacket returned without process abort
SPATEM_UNKNOWN_SENDER_RUNTIME_EXIT=0

A rejeição ocorre antes da criação de PacketContext.

Portanto esta evidência é classificada como:

routing/source identity rejection

e não como RX_PIPELINE DROP.

## R-13 — Duplicate content / replay

Execução real com duas transmissões SPATEM semanticamente idênticas:

first packet  -> SPATEM_REPLAY_OK
second packet -> SPATEM_REPLAY_DROP

Contagens:

SPATEM dispatch = 2
SPATEM_REPLAY_OK = 1
SPATEM_REPLAY_DROP = 1
SPATEM_RECEPTION_OK = 1
SPATEM_DELIVER_OK = 1

Evidência de fingerprint:

duplicate fingerprint station=1 intersection=42

Conclusão:

primeiro pacote -> RECEPTION -> DELIVER
segundo pacote  -> REPLAY_DROP
## R-14 — Independent physical duplicate

Este requisito não deve ser marcado como GREEN.

O mecanismo atual de replay utiliza identidade semântica e fingerprint do conteúdo.

PacketContext.packetId é um identificador local de observabilidade. Não é um identificador físico único do frame recebido.

Logo, foi demonstrado:

duplicate content -> replay rejection

mas não:

same physical frame -> independent duplicate-frame rejection

Estado:

LIMITATION

Não adicionar uma camada artificial de duplicate detection sem existir no modelo uma identidade física adequada.

## R-15 — Unknown configured intersection reference

O RX SPATEM atual não possui uma registry semântica de interseções configuradas usada para validar:

intersectionId ∈ configured_intersections

O caso testado:

intersectionId == 0

demonstra apenas um identificador inválido.

Não demonstra:

unknown configured intersection reference

Estado:

LIMITATION

Não introduzir uma registry de interseções apenas para satisfazer a matriz de robustez sem um requisito arquitetural correspondente.

## Arquitetura de identidade

A validação mantém as autoridades já estabelecidas:

VehicleIdentity
    -> autoridade da identidade do veículo

VanetzaRuntime
    -> autoridade do GN_ADDR e resolução da origem recebida

VehicleRegistry
    -> índices reversos de veículos

Link MAC
    != GN_ADDR
    != stationId

O SPATEM não deve usar VehicleRegistry::GetByStationId() como registry de RSU/interseções.

## Arquivos de implementação alterados
scratch/vanetza_integration/spatem/rx/spatem_rx_pipeline.cc
scratch/vanetza_integration/vanetza_runtime/vanetza_runtime.cc
scratch/vanetza_integration/wrappers/vanetza/spatem/spatem_asn1_builder.cc
scratch/spatem_real_replay_test/spatem_real_replay_test.cc
.gitignore
## Testes preservados para versionamento
scratch/spatem_codec_boundary_test/spatem_codec_boundary_test.cc
scratch/spatem_rx_pipeline_test/spatem_rx_pipeline_test.cc
scratch/spatem_unknown_sender_runtime_test/spatem_unknown_sender_runtime_test.cc

Os três testes foram explicitamente removidos da regra global de ignore através de exceções no .gitignore.

## Evidência executada

Regressão focada:

CODEC=0
RX_PIPELINE=0
UNKNOWN_SENDER=0
WRONG_BTP_PORT=0
REAL_RX=0
REAL_REPLAY=0

Wrong BTP port:

BUILD_RC=0
TEST_RC=0
PASS wrong BTP port is rejected at BTP_RX
PASS reason=invalid_spatem_btp_port
PASS no VERIFY/DECODE/REPLAY/RECEPTION/DELIVER

Wrong message ID:

BUILD_RC=0
TEST_RC=0
PASS wrong SPATEM messageID is rejected

git diff --check permaneceu sem erros.

## Git state

Branch:

feature/spatem-sumo-9-13

HEAD observado:

15ed197 (HEAD -> feature/spatem-sumo-9-13)
fix(spatem): finalize SPATEM acceptance validation

O worktree contém alterações locais ainda não commitadas.

## Critério de fechamento

A Fase 9.14 está tecnicamente concluída quando:

os testes GREEN registados permanecem reprodutíveis;
os três testes atualmente não rastreados são incluídos intencionalmente no próximo commit;
R-14 e R-15 permanecem explicitamente classificados como limitações;
nenhuma alteração adicional de produção é introduzida apenas para aumentar artificialmente a cobertura.
## Relações

[[ETAPA_9_SPATEM_PLANO]]

[[ETAPA_9_SPATEM_REBASELINE_2026-10-08]]

[[CURRENT_STATE]]

[[DOCUMENT_STATUS]]
