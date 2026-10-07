---

title: "Etapa 9 — SPATEM — Plano de Implementação e Validação"
status: "PROPOSED"
type: "FUNCTIONAL DEVELOPMENT AND VALIDATION PLAN"
date: 2026-10-04
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
stage: "ETAPA 9"
scope: "SPATEM"
baseline: "POST-P21"
baseline_commit: "420db8a"
previous_stage: "P21 CLOSED / VALIDATED"
next_stage: "SPATEM — PENDING PLAN APPROVAL"
related_state:

* "[[CURRENT_STATE]]"
  related_status:
* "[[DOCUMENT_STATUS]]"
  previous_rebaseline:
* "[[POST_P21_REBASELINE_2026-10-04]]"
  reference_plan:
* "[[ETAPA_8_DENM_PLANO]]"
  reference_state:
* "[[ETAPA_8_DENM_ESTADO_TECNICO_2026-09-18]]"

---

# Etapa 9 — SPATEM — Plano de Implementação e Validação

## 1. Objetivo

A Etapa 9 tem como objetivo introduzir suporte funcional a **SPATEM — Signal Phase And Timing Extended Message** na integração atual entre ns-3, Vanetza e SUMO.

O objetivo principal é obter um primeiro fluxo SPATEM ponta-a-ponta real:

```text
signal-state source
    -> SPATEM generation
    -> ASN.1 / UPER
    -> security
    -> BTP
    -> GeoNetworking
    -> ns-3 access / channel
    -> GeoNetworking RX
    -> BTP RX
    -> security verification
    -> decode
    -> semantic validation
    -> reception
    -> delivery
```

A implementação deverá reutilizar a arquitetura consolidada para CAM e DENM.

Não deverá ser criada uma arquitetura SPATEM paralela para identidade, `PacketContext`, logging, segurança, analyzer ou transporte.

## 2. Princípios de implementação

Mantêm-se os princípios demonstrados na Etapa 8:

```text
código plausível ≠ comportamento demonstrado
build limpo ≠ funcionalidade validada
run local ≠ interoperabilidade externa
```

A sequência operacional será incremental:

```text
audit
    -> model
    -> timing semantics
    -> codec
    -> transport
    -> TX
    -> RX
    -> security / replay
    -> logging
    -> analyzer
    -> E2E
    -> SUMO
    -> regression
    -> closeout
```

Nenhuma alteração estrutural significativa deverá avançar sem evidência da fase anterior.

Cada item técnico deverá seguir:

```text
problema / requisito
    -> evidência atual
    -> teste RED focado
    -> alteração mínima
    -> GREEN
    -> acceptance
    -> commit separado
```

## 3. Estado de partida

A Etapa 9 parte do rebaseline pós-P21:

```text
main
HEAD = 420db8a
P21 = CLOSED / VALIDATED
working tree = CLEAN
```

O projeto já possui arquitetura comum para:

```text
CAM
DENM
BTP
GeoNetworking
DCC
security
PacketContext
RX pipeline
logging
v2x_analyzer
SUMO / bridge
```

A integração atual mantém separação entre domínio da aplicação e wrappers Vanetza.

A estrutura conceptual existente inclui:

```text
scratch/vanetza_integration/
├── cam/
├── denm/
├── context/
├── rx/
├── security/
├── sumo_adapter/
├── vanetza_runtime/
├── wrappers/
│   └── vanetza/
└── tests/
```

SPATEM deverá encaixar nessa estrutura.

## 4. Escopo

O primeiro marco será exclusivamente:

```text
SPATEM baseline funcional
```

O baseline deverá demonstrar:

```text
SPATEM generation
SPATEM ASN.1 construction
UPER encode
BTP transport
GeoNetworking transport
TX runtime
RX runtime
UPER decode
semantic validation
security path
replay handling
PacketContext
logging
analyzer
E2E
```

Ficam fora do primeiro baseline:

```text
MAPEM implementation
SREM / SSEM
controle de semáforos reais
interoperabilidade OTA
conformance certification
hardware ITS-G5 real
```

A relação SPATEM/MAPEM poderá ser tratada posteriormente. O primeiro objetivo é demonstrar SPATEM como mensagem funcional sem aumentar prematuramente o escopo.

## 5. Questão arquitetural específica: origem SPATEM

Antes de implementar SPATEM deverá ser resolvida uma questão que não existe da mesma forma no CAM/DENM atual:

```text
SPATEM originator
    =
ITS station / roadside infrastructure / signal controller
```

O projeto atual tem uma arquitetura de identidade fortemente orientada para estações móveis/veículos.

Portanto, a Etapa 9 não deverá simplesmente assumir:

```text
SPATEM = CAM de um veículo com outro payload
```

Deverá existir uma decisão explícita sobre a representação do originador SPATEM.

O primeiro gate deverá determinar se:

```text
A) o modelo de identidade existente suporta corretamente uma estação de infraestrutura
```

ou:

```text
B) é necessária uma generalização mínima do modelo de identidade
```

sem quebrar:

```text
VehicleIdentity
stationId
GN_ADDR
credential binding
PacketContext
```

Nenhum código SPATEM dependente desta decisão deverá ser desenvolvido antes do encerramento deste gate.

## 6. Sequência oficial

A sequência proposta é:

```text
9.0  Freeze / verify post-P21 baseline
 |
9.1  Audit installed Vanetza SPATEM API
 |
9.2  Audit SPATEM origin identity / station role
 |
9.3  Define neutral SPATEM data model
 |
9.4  Define timing and signal-state model
 |
9.5  Implement ASN.1 / UPER codec
 |
9.6  Integrate BTP / GeoNetworking transport
 |
9.7  Implement SPATEM TX service
 |
9.8  Implement SPATEM RX
 |
9.9  Integrate security / replay
 |
9.10 Align PacketContext and logging
 |
9.11 Extend v2x_analyzer
 |
9.12 Synthetic SPATEM E2E
 |
9.13 SUMO traffic-light integration
 |
9.14 Robustness / multi-intersection validation
 |
9.15 CAM + DENM regression
 |
9.16 Closeout
```

A ordem poderá ser alterada apenas por dependência técnica demonstrável.

## 7. Fase 9.0 — Freeze / verify post-P21 baseline

### Objetivo

Garantir que o ponto de partida SPATEM é reproduzível.

### Evidência

Registar:

```text
git commit atual
git status
./ns3 build
estado do analyzer
CAM baseline
DENM baseline
P21 acceptance baseline
```

### Critério de saída

O baseline deverá ser identificado por:

```text
HEAD = 420db8a
main = origin/main
working tree = CLEAN
```

Nenhuma regressão anterior poderá ser atribuída à introdução de SPATEM.

## 8. Fase 9.1 — Auditoria da API SPATEM instalada

Esta fase é obrigatoriamente anterior à implementação.

Não deverão ser assumidos nomes de classes, aliases, converters ou builders.

A auditoria deverá pesquisar localmente:

```text
SPATEM
SPAT
SignalPhaseAndTiming
IntersectionState
IntersectionReferenceID
MovementState
MovementEvent
SignalGroup
TimeChangeDetails
ItsPduHeader
```

Também deverá verificar:

```text
ASN.1 generated types
UPER encoder
UPER decoder
Vanetza aliases
facilities APIs
BTP ports
ITS-AID
message ID
protocol version
GeoNetworking transport support
security integration
```

Deverá produzir uma matriz:

| Elemento           | API/header local | Estado |
| ------------------ | ---------------- | ------ |
| SPATEM root        | TBD              | TBD    |
| SPAT               | TBD              | TBD    |
| Intersection state | TBD              | TBD    |
| Movement state     | TBD              | TBD    |
| Signal group       | TBD              | TBD    |
| UPER encoder       | TBD              | TBD    |
| UPER decoder       | TBD              | TBD    |
| BTP SPATEM port    | TBD              | TBD    |
| ITS-AID            | TBD              | TBD    |
| GeoNetworking      | TBD              | TBD    |
| security           | TBD              | TBD    |

### Gate

Se a instalação não fornecer uma API SPATEM utilizável, a implementação deverá parar neste ponto e produzir uma decisão técnica separada.

## 9. Fase 9.2 — Origem, identidade e station role

### Objetivo

Definir o originador lógico de SPATEM sem contaminar a arquitetura existente.

Deverá ser separado:

```text
stationId
GN_ADDR.MID
Link MAC
credential identity
signal-controller role
```

A regra permanece:

```text
Link MAC ≠ GN_ADDR.MID ≠ stationId
```

A aceitação deverá demonstrar que o originador SPATEM não é inferido erradamente a partir do hop sender.

O `PacketContext` deverá continuar a identificar o pacote físico independentemente da identidade semântica da estação.

## 10. Fase 9.3 — Modelo de dados neutro

O domínio da aplicação não deverá depender diretamente da ASN.1.

O modelo conceptual inicial deverá representar, conforme suportado pela API local:

```text
SpatemInput
├── station identity
├── intersection reference
├── intersection revision
├── intersection status
├── reference / generation time
├── movement states
├── signal group
├── phase / event state
├── timing information
└── optional regulatory / topology information
```

Os nomes e campos finais serão determinados após a auditoria.

A fronteira deverá ser:

```text
signal-state application
        |
        v
SpatemInput / SpatemMessage
        |
        v
SPATEM wrapper
        |
        v
ASN.1 / UPER
```

## 11. Fase 9.4 — Modelo temporal e estado dos sinais

Esta será uma fase própria porque o elemento central do SPATEM não é apenas a estrutura ASN.1: é a semântica temporal.

Deverá ser definido:

```text
simulation time
    ->
SPATEM protocol time representation
```

e:

```text
signal state
    ->
phase/event state
    ->
start/end/change timing
```

O primeiro modelo deverá ser determinístico.

Exemplo conceptual:

```text
RED
 |
 | duration
 v
GREEN
 |
 | duration
 v
YELLOW
 |
 v
RED
```

O modelo deverá permitir:

```text
phase transitions
timing intervals
signal-group association
intersection revision
periodic publication
```

As conversões de unidades e quantização temporal deverão ser testadas isoladamente.

Não deverá haver lógica temporal escondida dentro do encoder ASN.1.

## 12. Fase 9.5 — ASN.1 / UPER codec

O codec deverá permanecer dentro da fronteira Vanetza:

```text
scratch/vanetza_integration/wrappers/vanetza/
    spatem/
```

A estrutura conceptual será:

```text
spatem/
├── spatem_asn1_builder
├── spatem_codec
└── converter
```

Os nomes finais dependem da API local.

### Teste mínimo

```text
SpatemInput
    -> encode
    -> bytes
    -> decode
    -> SpatemMessage
```

A aceitação deverá demonstrar round-trip dos campos essenciais:

```text
intersection
revision
signal group
phase state
timing
station identity
```

sem perda semântica.

## 13. Fase 9.6 — BTP / GeoNetworking

SPATEM deverá utilizar a abstração comum de transporte já estabelecida no projeto.

Não deverá existir:

```text
SpatemBtpAdapter
```

copiado estruturalmente do CAM.

Deverá ser reutilizado:

```text
BtpTxRequest
PacketContext
GeoNetworking wrapper
access adapter
DCC path
```

A porta BTP e o ITS-AID não deverão ser hard-coded antes da auditoria 9.1.

A escolha do transporte:

```text
SHB / GBC / outro
```

deverá ser explicitamente documentada e justificada pelo caso de uso implementado.

## 14. Fase 9.7 — SPATEM TX

O fluxo TX deverá ser:

```text
SIGNAL_STATE_CHANGE / SCHEDULER
        ->
GENERATE
        ->
ENCODE
        ->
SIGN
        ->
BTP_TX
        ->
GEONET_TX
        ->
ACCESS_TX
        ->
SEND
        ->
TX_COMPLETE
```

O primeiro cenário será um único cruzamento determinístico.

Não deverá depender inicialmente de SUMO.

A primeira aceitação deverá demonstrar:

```text
SPATEM generation
message ID correcto
station ID correcto
intersection state correcto
signal group correcto
timing correcto
TX_COMPLETE RESULT=OK
```

## 15. Fase 9.8 — SPATEM RX

O RX utilizará a infraestrutura comum:

```text
RAW_RX
    ->
ACCESS_RX
    ->
GEONET_RX
    ->
BTP_RX
    ->
VERIFY
    ->
DECODE
    ->
REPLAY
    ->
RECEPTION
    ->
DELIVER
```

O dispatch deverá distinguir:

```text
CAM
DENM
SPATEM
```

sem perder os handlers existentes.

O receiver deverá validar pelo menos:

```text
message type
station identity
intersection identity
revision
signal groups
timing fields
```

A validação não deverá usar apenas a porta BTP quando o `messageID` puder ser verificado no payload.

## 16. Fase 9.9 — Security / Replay

SPATEM deverá reutilizar a infraestrutura de segurança existente.

Não deverá ser criada uma implementação criptográfica específica para SPATEM sem necessidade técnica demonstrada.

A aceitação deverá separar:

```text
security source evidence
security build evidence
security runtime evidence
```

e distinguir explicitamente:

```text
signature generation
signature verification
replay acceptance
replay rejection
```

Uma implementação de código sem observação runtime não será considerada prova.

## 17. Fase 9.10 — PacketContext e logging

SPATEM deverá utilizar a mesma taxonomia global.

TX:

```text
GENERATE
ENCODE
SIGN
BTP_TX
GEONET_TX
ACCESS_TX
SEND
TX_COMPLETE
```

RX:

```text
RAW_RX
ACCESS_RX
GEONET_RX
BTP_RX
VERIFY
DECODE
REPLAY
RECEPTION
DELIVER
```

Eventos específicos poderão existir como informação auxiliar:

```text
SPATEM_GENERATE_OK
SPATEM_DECODE_OK
SPATEM_RECEPTION_OK
SPATEM_DELIVER_OK
SPATEM_INVALID_TIMING
SPATEM_UNKNOWN_INTERSECTION
```

mas não deverão formar uma segunda taxonomia.

O `PacketContext` será a fonte canónica de observabilidade por pacote.

`packetId` deverá manter a semântica física estabelecida na validação do analyzer.

## 18. Fase 9.11 — v2x_analyzer

O analyzer deverá reconhecer explicitamente:

```text
CAM
DENM
SPATEM
```

mantendo a arquitetura comum.

As métricas mínimas deverão cobrir:

```text
TX
├── GENERATE
├── ENCODE
├── SIGN
├── BTP_TX
├── GEONET_TX
├── ACCESS_TX
└── TX_COMPLETE

RX
├── RAW_RX
├── ACCESS_RX
├── GEONET_RX
├── BTP_RX
├── VERIFY
├── DECODE
├── REPLAY
├── RECEPTION
└── DELIVER
```

e métricas SPATEM específicas:

```text
intersection count
movement count
signal-group count
decode failures
timing validation failures
replay drops
logical receptions
physical packet duplicates
```

A semântica de duplicados deverá continuar baseada em identidade física de pacote, não em estado lógico SPATEM.

Os testes existentes do analyzer deverão continuar a passar.

## 19. Fase 9.12 — Synthetic SPATEM E2E

Antes de integrar SUMO, deverá existir um cenário puramente determinístico:

```text
Infrastructure Station A
        |
        | SPATEM
        v
ns-3 channel
        |
        v
Vehicle / ITS Station B
```

A evidência TX deverá demonstrar:

```text
GENERATE
ENCODE
SIGN
BTP_TX
GEONET_TX
ACCESS_TX
SEND
TX_COMPLETE
```

A evidência RX deverá demonstrar:

```text
RAW_RX
ACCESS_RX
GEONET_RX
BTP_RX
VERIFY
DECODE
REPLAY
RECEPTION
DELIVER
```

Além disso deverá ser possível verificar semanticamente:

```text
intersection ID
revision
signal group
phase state
timing
origin station
```

Este será o primeiro marco E2E oficial da Etapa 9.

## 20. Fase 9.13 — Integração SUMO

Somente depois do SPATEM sintético estar validado deverá ser ligada a fonte real de estado de tráfego.

Fluxo:

```text
SUMO traffic light state
        |
        v
sumo_adapter / bridge
        |
        v
SPATEM domain model
        |
        v
SPATEM scheduler/service
        |
        v
Vanetza / ns-3
```

O primeiro objetivo será uma única interseção.

Posteriormente poderão ser introduzidos:

```text
multiple intersections
multiple signal groups
phase transitions
revision changes
```

A introdução do SUMO não deverá alterar o caminho comum de transporte.

## 21. Fase 9.14 — Robustez e validação estrutural

Deverão ser testados pelo menos:

```text
invalid intersection reference
invalid revision
invalid signal group
invalid timing
missing mandatory state
unsupported phase state
malformed encoded payload
wrong message ID
wrong BTP dispatch
duplicate physical packet
replay event
unknown sender
self-origin behavior
```

O teste deverá distinguir:

```text
decode failure
semantic validation failure
replay rejection
routing failure
delivery rejection
```

Não deverão ser agregados todos estes casos sob um único `FAIL`.

## 22. Fase 9.15 — Regressão CAM + DENM

A integração SPATEM só poderá avançar para closeout depois de demonstrar que não degradou CAM e DENM.

Deverão ser executados:

```text
CAM-only
DENM-only
SPATEM-only
CAM + DENM
CAM + SPATEM
DENM + SPATEM
CAM + DENM + SPATEM
```

quando o cenário permitir.

Deverão ser mantidos:

```text
CAM BTP port
DENM BTP port
SPATEM BTP port
PacketContext semantics
TX/RX taxonomy
DCC behavior
security behavior
analyzer behavior
```

## 23. Fase 9.16 — Closeout

A Etapa 9 será considerada CLOSED somente quando:

```text
SPATEM API audit             PASS
origin identity model        PASS
neutral model                PASS
timing model                 PASS
ASN.1 / UPER                 PASS
BTP / GN                     PASS
TX runtime                   PASS
RX runtime                   PASS
security                     PASS / explicitly bounded
replay                       PASS / explicitly bounded
PacketContext                PASS
logging                      PASS
analyzer                     PASS
synthetic E2E                PASS
SUMO integration             PASS
robustness                   PASS
CAM regression               PASS
DENM regression              PASS
build                         PASS
runtime evidence              PASS
documentation                 PASS
```

## 24. Matriz de aceitação

| ID        | Critério          | Evidência esperada               | Estado inicial |
| --------- | ----------------- | -------------------------------- | -------------- |
| SPATEM-01 | Post-P21 baseline | `420db8a`, clean, build          | ⏳              |
| SPATEM-02 | Vanetza API       | headers/símbolos locais          | ⏳              |
| SPATEM-03 | Origin identity   | station role demonstrado         | ⏳              |
| SPATEM-04 | Neutral model     | `SpatemInput` / `SpatemMessage`  | ⏳              |
| SPATEM-05 | Timing model      | state/transition tests           | ⏳              |
| SPATEM-06 | ASN.1             | valid SPATEM structure           | ⏳              |
| SPATEM-07 | UPER              | encode/decode round-trip         | ⏳              |
| SPATEM-08 | BTP               | port/payload verified            | ⏳              |
| SPATEM-09 | GeoNetworking     | transport verified               | ⏳              |
| SPATEM-10 | TX                | runtime TX complete              | ⏳              |
| SPATEM-11 | RX                | runtime RX complete              | ⏳              |
| SPATEM-12 | Security          | generation/verification evidence | ⏳              |
| SPATEM-13 | Replay            | duplicate rejection evidence     | ⏳              |
| SPATEM-14 | PacketContext     | packet continuity verified       | ⏳              |
| SPATEM-15 | Logging           | global taxonomy preserved        | ⏳              |
| SPATEM-16 | Analyzer          | SPATEM metrics/tests             | ⏳              |
| SPATEM-17 | Synthetic E2E     | infrastructure → receiver        | ⏳              |
| SPATEM-18 | SUMO TLS          | real traffic-light state         | ⏳              |
| SPATEM-19 | Robustness        | invalid-input matrix             | ⏳              |
| SPATEM-20 | CAM regression    | existing CAM baseline preserved  | ⏳              |
| SPATEM-21 | DENM regression   | existing DENM behavior preserved | ⏳              |
| SPATEM-22 | Build             | `./ns3 build` PASS               | ⏳              |
| SPATEM-23 | Runtime evidence  | logs/counts/semantic checks      | ⏳              |
| SPATEM-24 | Closeout          | documentation complete           | ⏳              |

## 25. Evidência mínima

Cada fase deverá preservar:

```text
SOURCE EVIDENCE
    -> código / API / diff

BUILD EVIDENCE
    -> compile / link

RUNTIME EVIDENCE
    -> comportamento observável
```

Quando aplicável, guardar:

```text
comandos
headers encontrados
símbolos
diff
testes RED
testes GREEN
build
logs
contagens
PacketContext
packetId
analyzer report
E2E result
SUMO correlation
```

Nenhum critério deverá ser marcado PASS apenas porque uma classe ou adapter existe.

## 26. Riscos principais

### 26.1 API SPATEM inexistente na instalação

Mitigação:

```text
9.1 audit gate
```

### 26.2 Tratar SPATEM como mensagem de veículo

Mitigação:

```text
9.2 identity / station-role gate
```

### 26.3 Misturar simulation time com protocol time

Mitigação:

```text
9.4 dedicated timing model
```

### 26.4 Hard-code de BTP port / ITS-AID

Mitigação:

```text
audit local + evidence from installed API
```

### 26.5 Copiar CAM/DENM sem modelar semântica de interseção

Mitigação:

```text
neutral SPATEM model
signal-state model
```

### 26.6 Tornar MAPEM uma dependência prematura

Mitigação:

```text
SPATEM baseline sem MAPEM
MAPEM apenas numa fase posterior se requisito demonstrado
```

### 26.7 Quebrar CAM ou DENM

Mitigação:

```text
dedicated regression gates
```

### 26.8 Validar apenas por build

Mitigação:

```text
synthetic E2E + SUMO E2E
```

## 27. Organização esperada

A estrutura conceptual deverá seguir o padrão já consolidado:

```text
scratch/vanetza_integration/
├── spatem/
│   ├── types/
│   ├── builder/
│   ├── timing/
│   ├── scheduler/
│   ├── sender/
│   └── reception/
│
├── wrappers/
│   └── vanetza/
│       └── spatem/
│
├── context/
├── rx/
├── security/
├── vanetza_runtime/
├── sumo_adapter/
├── tests/
└── docs/
```

Os nomes reais deverão seguir a convenção do código existente quando isso evitar duplicação estrutural.

## 28. Estratégia de commits

Cada fase funcional aprovada deverá gerar um commit separado.

Exemplo:

```text
docs(spatem): define implementation plan
test(spatem): establish timing model acceptance
feat(spatem): add neutral data model
test(spatem): validate codec roundtrip
feat(spatem): add codec
...
```

Não deverá existir um único commit gigante para toda a Etapa 9.

Cada commit deverá corresponder a uma alteração tecnicamente verificável.

## 29. Definição de DONE

SPATEM DONE significa:

```text
application state
    ->
valid SPATEM
    ->
UPER
    ->
security
    ->
BTP
    ->
GeoNetworking
    ->
ns-3 PHY/channel
    ->
GeoNetworking RX
    ->
BTP RX
    ->
verification
    ->
decode
    ->
semantic validation
    ->
delivery
```

com:

```text
PacketContext
logging
analyzer
synthetic E2E
SUMO E2E
robustness
CAM regression
DENM regression
build evidence
runtime evidence
```

Interoperabilidade externa permanece uma dimensão separada.

## 30. Gate para início da implementação

Nenhum código SPATEM deverá ser criado antes de concluir:

```text
9.0  baseline verified
9.1  installed SPATEM API audited
9.2  origin identity / station role decided
```

A primeira implementação efetiva deverá começar somente depois desses três gates estarem demonstrados.

## 31. Decisão proposta

A Etapa 9 — SPATEM — é proposta como o próximo ciclo técnico após o P21.

O rebaseline pós-P21 não deverá ser reescrito retroativamente.

A aprovação da presente especificação deverá resultar numa nova atualização documental:

```text
P21 = CLOSED / VALIDATED
NEXT = SPATEM
ETAPA 9 = APPROVED
```

Essa atualização deverá ser feita em commit documental próprio antes da primeira alteração funcional de código.
