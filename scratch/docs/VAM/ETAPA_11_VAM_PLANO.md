---
title: "Etapa 11 — VAM — Plano de Implementação e Validação"
status: "PROPOSED"
type: "FUNCTIONAL DEVELOPMENT AND VALIDATION PLAN"
date: 2026-11-03
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
stage: "ETAPA 11"
scope: "VAM — VRU Awareness Message / VRU Basic Service"
baseline: "POST-CPM-CLOSEOUT"
previous_stage: "ETAPA 10 — CPM / CLOSEOUT REQUIRED"
next_stage: "VAM — PENDING PLAN APPROVAL"
related_plan:
  - "[[ETAPA_8_DENM_PLANO]]"
  - "[[ETAPA_9_SPATEM_PLANO]]"
  - "[[ETAPA_10_CPM_PLANO]]"
related_state:
  - "[[CURRENT_STATE]]"
  - "[[DOCUMENT_STATUS]]"
---

# Etapa 11 — VAM — Plano de Implementação e Validação

## 1. Objetivo

A Etapa 11 tem como objetivo introduzir suporte funcional a **VAM — VRU Awareness Message** e ao respetivo **VBS — VRU Basic Service** na integração existente entre ns-3.47, Vanetza e SUMO.

O objetivo principal é demonstrar que uma VRU ITS-S equipada consegue gerir o seu papel, construir e assinar um VAM válido, disseminá-lo pelo caminho BTP/GeoNetworking, e que outra ITS-S consegue receber, verificar, descodificar, validar a plausibilidade e disponibilizar a informação à aplicação/LDM.

O primeiro caso de uso será deliberadamente limitado a um **peão equipado (VRU Profile 1) numa aproximação/travessia de cruzamento**, com uma estação emissora e pelo menos uma estação recetora. A escolha reduz a ambiguidade inicial de perfil, identidade, cinemática e comportamento de transmissão. Biciclistas/e-scooters, trabalhadores em zona de obras, VRU clusters e cenários com várias VRUs serão acrescentados por fases.

O fluxo funcional pretendido é:

```text
VRU profile / role management + PoTi / device data
    -> VRU Basic Service (VBS)
    -> generation management / triggering / redundancy mitigation
    -> neutral VAM model
    -> Vanetza VAM builder / ASN.1 UPER codec
    -> ITS security (AID 638 / VAM SSP / certificate)
    -> BTP-B port 2018
    -> GeoNetworking SHB
    -> ns-3 access / channel
    -> GeoNetworking / BTP RX
    -> security verification
    -> VAM decode
    -> relevance / integrity / plausibility checks
    -> replay and freshness handling
    -> VBS receiver / LDM update
    -> ITS application delivery
```

O VAM deverá integrar-se na arquitetura consolidada do projeto. Não deverá criar uma segunda arquitetura paralela para transporte, segurança, identidade, `PacketContext`, logging, receção ou análise.

## 2. Princípios de implementação e validação

Mantêm-se os princípios das Etapas 8 e 10:

```text
código plausível != comportamento demonstrado
build limpo != funcionalidade validada
run local != interoperabilidade externa
codec round-trip != VBS funcional
mensagem assinada != autorização válida
```

Cada unidade de trabalho deverá seguir a sequência:

```text
requisito / risco
    -> evidência do estado atual
    -> teste RED focado
    -> alteração mínima
    -> teste GREEN
    -> acceptance observável
    -> evidência guardada
    -> commit separado
```

Nenhum gate será marcado `PASS` apenas porque existe uma classe, wrapper, codec, adapter ou chamada de API. A aceitação exige evidência adequada de fonte, build e runtime.

A implementação deverá ainda respeitar estes princípios específicos:

1. **VAM é um serviço de facilities.** A implementação deve conter a lógica VBS (papel, estados, triggers, geração, clustering e receção), não apenas a serialização de um PDU.
2. **A origem é uma VRU ITS-S equipada.** A deteção de uma VRU não equipada por um veículo não deve ser apresentada como transmissão VAM originada por essa VRU; esse caso pertence a serviços como CPM/DENM, conforme o cenário.
3. **A geração é orientada por estado e congestionamento.** Não implementar uma periodicidade fixa arbitrária como substituto da política VAM.
4. **Segurança é obrigatória.** Não aceitar um caminho VAM sem assinatura/verificação e sem credenciais autorizadas para VRU Awareness.
5. **Privacidade faz parte do modelo.** Não transformar `stationId`, GN_ADDR ou um identificador SUMO estável num identificador pessoal persistente.
6. **A informação recebida tem validade temporal.** Um VAM antigo não pode manter indefinidamente uma VRU na LDM.
7. **Clusters representam uma entidade lógica.** Os membros passivos não podem continuar a emitir VAM individual enquanto integram um cluster ativo, salvo transições explicitamente previstas pelo protocolo.

## 3. Referência normativa, versões e decisões de baseline

### 3.1 Referência inicial

A referência documental publicada para esta etapa será **ETSI TS 103 300-3 V2.3.1 (2025-12), Release 2**, complementada pelas versões compatíveis da TS 103 300-2, CDD e módulos ASN.1. O plano não assume que a instalação local do Vanetza já contém esta versão.

A auditoria deverá confirmar a combinação real de:

- edição ETSI e ASN.1 VAM;
- CDD e respetivas versões/imports;
- namespaces e headers gerados;
- encoder/decoder UPER;
- bibliotecas Vanetza ligadas ao executável de produção;
- ITS-AID, SSP e certificado disponíveis;
- BTP destination port e composição GeoNetworking;
- release efetivamente usada pelos restantes protocolos.

A especificação TS 103 300-3 define `protocolVersion = 3`, `messageID = vam(16)` e os elementos principais do PDU. O módulo ASN.1 e o CDD têm de corresponder à release escolhida; não misturar módulos de versões diferentes. [ETSI TS 103 300-3 V2.3.1](https://www.etsi.org/deliver/etsi_ts/103300_103399/10330003/02.03.01_60/ts_10330003v020301p.pdf).

### 3.2 Evolução normativa em curso

A pesquisa ETSI identificou o work item **RTS/ITS-0011009**, com draft V0.0.6 datado de 2026-10-09, relativo à atualização da TS 103 300-3. O draft propõe alterações à geração/disseminação VAM e revê o enquadramento de bicicletas e veículos de duas rodas motorizados face ao CAM Release 2.

Este draft não substitui a norma publicada. A fase 11.1 deverá verificar se foi publicada uma nova edição até à data de congelamento do perfil. Se continuar em draft, a implementação deverá usar a edição publicada e documentar as diferenças que possam afetar uma atualização futura. Não se deve implementar silenciosamente comportamento de draft como requisito normativo. [ETSI work item RTS/ITS-0011009](https://portal.etsi.org/webapp/WorkProgram/Report_WorkItem.asp?WKI_ID=75431).

### 3.3 Parâmetros de integração identificados na pesquisa

Os valores abaixo são a referência documental para o primeiro perfil de comunicação direta. Devem ser confirmados contra as fontes locais e a configuração do ITS-S antes de serem congelados no código.

| Elemento | Referência inicial | Regra de implementação |
|---|---:|---|
| ITS message ID | `vam(16)` | Confirmar enumeração ASN.1 e dispatch |
| `protocolVersion` | `3` | Derivar da edição aprovada |
| BTP destination port | `2018` | ETSI TS 103 248 V2.5.1; confirmar no BTP local |
| BTP header | Type B | Utilizar o tipo definido pelo perfil/configuração |
| GeoNetworking transport | SHB | Perfil de comunicação direta VAM |
| ITS-AID | `638` | VRU Awareness Basic Service; não usar AID CAM/DENM/CPM |
| GN traffic class | Igual à configuração CAM | Reutilizar a política global do projeto |
| GN maximum packet lifetime | `<= 1000 ms` | Expiração e validação observáveis |
| Security | `SECURED` | Não permitir fallback para VAM sem segurança |

Fontes: [ETSI TS 103 248 V2.5.1](https://www.etsi.org/deliver/etsi_ts/103200_103299/103248/02.05.01_60/ts_103248v020501p.pdf), [ETSI TS 102 965 V2.2.1](https://www.etsi.org/deliver/etsi_ts/102900_102999/102965/02.02.01_60/ts_102965v020201p.pdf) e TS 103 300-3 V2.3.1.

### 3.4 Matriz de decisões normativas obrigatória

| Decisão | Resultado exigido | Estado inicial |
|---|---|---|
| Edição TS 103 300-3 | Published version / eventual revisão mais recente verificada | TBD |
| Edição TS 103 300-2 | Perfil VRU e regras de gestão compatíveis | TBD |
| ASN.1 VAM | Módulo, OID, hash e origem confirmados | TBD |
| CDD | Versão/imports compatíveis | TBD |
| ITS-AID | 638 para VRU Awareness | TBD |
| BTP | 2018, Type B | TBD |
| GeoNetworking | SHB / lifetime / traffic class confirmados | TBD |
| Security | ITS certificate, VAM SSP e assinatura GeoNetworking | TBD |
| VRU profile inicial | Profile 1 — Pedestrian | PROPOSED |
| Caso de uso inicial | UC-B2 — VRU pedestrian at crossroads | PROPOSED |

**Critério de saída:** não começar implementação dependente de ASN.1, AID, SSP ou transporte sem fechar esta matriz com evidência da especificação e da instalação real.

## 4. Gate de entrada e estado de partida

### 4.1 Gate obrigatório

A Etapa 11 fica proposta como plano. A implementação funcional VAM não deverá começar antes de a Etapa 10 estar formalmente `CLOSED / VALIDATED` ou de existir uma decisão documentada que identifique exatamente quais os gates CPM aceites e quais os que permanecem fora do âmbito.

Antes do primeiro commit funcional VAM deverá existir evidência de:

```text
Etapa 10 CPM closeout = CLOSED / VALIDATED
HEAD registado após closeout
branch e working tree documentados
working tree = CLEAN (ou exceções explicitamente preservadas)
production build = PASS
CAM baseline = PASS
DENM baseline = PASS
SPATEM baseline = PASS
CPM baseline = PASS
v2x_analyzer baseline = PASS
```

A informação histórica desta conversa não substitui a verificação do checkout atual. A fonte de verdade é o Git checkout atual e os testes que nele existem.

### 4.2 Freeze do baseline

Registar pelo menos:

```bash
git rev-parse HEAD
git status --short --branch
git log -1 --oneline
cmake --build cmake-cache --clean-first --target ns3.47-v2x_sumo-default -j2
python3 -m pytest scratch/v2x_analyzer/tests
```

Executar e arquivar os cenários runtime de CAM, DENM, SPATEM e CPM que constituem o baseline aceite. Usar os comandos reais documentados no estado atual do projeto; não inventar comandos de execução VAM antes de existir harness.

### 4.3 Limite de build

A configuração de produção de `scratch/CMakeLists.txt` deverá manter a restrição deliberada ao executável `v2x_sumo`. Não adicionar targets VAM de diagnóstico ao build de produção. Testes unitários e fixtures deverão usar o mecanismo de testes existente.

### Critério de saída

Baseline reprodutível, gates anteriores fechados e worktree controlado. Se a Etapa 10 ainda não estiver formalmente encerrada, a fase fica `BLOCKED` e nenhum código VAM é iniciado.

## 5. Casos de uso e âmbito funcional

A seleção de casos de uso determina os dados que a VRU precisa de publicar e os comportamentos a validar. VAM não deve ser tratado como uma mensagem genérica de posição sem relação com o contexto da pessoa/dispositivo.

### 5.1 Matriz de casos de uso

| ID / cenário | Origem e recetor | Necessidade VAM | Prioridade | Decisão para esta etapa |
|---|---|---|---|---|
| UC-B2 — Peão num cruzamento | Peão equipado -> veículo ITS-S / outra VRU | Posição, movimento e perfil do peão para awareness contextual | P0 | Caso de uso inicial; Profile 1 |
| UC-A2 — Peão e e-scooter em aproximação | Peão/e-scooter equipado -> outro VRU/veículo | Awareness de aproximação e trajetória; cenário de TTC reduzido no relatório | P1 | Peão primeiro; e-scooter depois de confirmar perfil 2 e regras CAM/VAM |
| UC-A1 — Peão e ciclista em passeio partilhado | VRUs equipadas -> VRUs próximas | Partilha de espaço, movimento relativo e awareness de proximidade | P1 | Cenário multi-VRU após E2E individual |
| UC-B1 — Trabalhadores em zona de obras | Dispositivos dos trabalhadores -> veículos próximos | Presença e movimento de trabalhadores na zona de trabalho | P1 | VAM combinado com contexto DENM de roadworks; não duplicar DENM |
| VRU cluster / grupo de peões | Cluster leader -> ITS-S próximas | Representação do grupo como entidade agregada | P1 | Obrigatório para closeout completo; fase posterior ao VAM individual |
| UC-C1 — VRU não equipada oculta por obstáculo | Veículo/sensor -> veículo/rede | Deteção indireta de VRU sem VAM originado pela pessoa | Fora do baseline | CPM/DENM conforme perfil do caso; não simular como VAM da VRU |
| UC-E1 — proteção assistida por rede | Centro/serviço -> veículos e VRUs | Agregação e distribuição através de terceiro | Futuro | Fora do primeiro baseline |
| Motorcyclist / Profile 3 | ITS-S de motociclo -> ITS-S próximas | Awareness cinemática coberta pelo CAM na edição publicada | Excluído da emissão VAM inicial | Não forçar VAM Profile 3; verificar alinhamento CAM R2 e draft em curso |
| Animal / Profile 4 | Dispositivo equipado no animal -> ITS-S próximas | Awareness animal quando existe ITS-S equipado | Futuro | Só testar com dispositivo originador explícito; deteção de animal não equipado não é VAM TX |

Os casos UC-A1, UC-A2, UC-B1, UC-B2, UC-C1 e UC-E1 são descritos no ETSI TR 103 300-1. O TR apresenta cenários ilustrativos; os campos e a semântica efetiva do protocolo devem continuar a ser derivados da TS 103 300-3 e da TS 103 300-2.

### 5.2 Perfil inicial

O primeiro emissor será uma **VRU ITS-S Profile 1 — Pedestrian** em `VRU_ROLE_ON`. A estação poderá ser um dispositivo pessoal (P-ITS-S) modelado no cenário ns-3.

A origem deve representar o estado do próprio peão equipado. Não deverá publicar o estado global de todos os peões no SUMO nem construir um VAM em nome de uma pessoa não equipada.

### 5.3 Tratamento dos outros perfis

- **Profile 2 — Bicyclist:** abrange bicicletas e vários light vehicles, incluindo e-scooters e dispositivos de mobilidade pessoal. A TS publicada deve ser interpretada em conjunto com as regras CAM e com a revisão ETSI em curso. Não presumir que todos os dispositivos Profile 2 emitem VAM no mesmo modo.
- **Profile 3 — Motorcyclist:** a edição publicada indica que motociclos não devem transmitir VAM porque a informação relevante já é transmitida por CAM. Não criar um transmissor VAM Profile 3 por conveniência.
- **Profile 4 — Animal:** só pode originar VAM quando existe uma ITS-S equipada no animal. A maioria dos animais não terá dispositivo e será detetada indiretamente; esse caminho não é VAM TX.

### Critério de saída

Documento de perfil aprovado, matriz de casos de uso com prioridades, atores, dados necessários, mensagem(s) de suporte e limitações. O perfil inicial não pode misturar a semântica de pedestrian, bicyclist e motorcyclist.

## 6. Arquitetura VAM dentro do projeto

### 6.1 Fronteira VBS

A VBS deverá ser uma entidade de facilities responsável por:

- aceitar notificações de VRU role/profile management;
- consultar PoTi para posição e tempo;
- consultar Device Data Provider / fonte de movimento;
- manter estado individual, cluster leader ou cluster passive;
- avaliar triggers e política de frequência;
- construir VAM e encaminhá-lo para segurança e N&T;
- receber VAM, verificar relevância, integridade e plausibilidade;
- atualizar a LDM e notificar aplicações superiores.

As interfaces locais entre VBS e aplicação podem ser adaptadas ao projeto, mas não devem espalhar tipos ASN.1 por SUMO, aplicações ou analyzer.

### 6.2 Arquitetura lógica

```text
SUMO / synthetic VRU data source
    -> VRU identity + profile / role management
    -> PoTi / device data provider
    -> VBS domain service
         +-> role and state machine
         +-> generation / trigger management
         +-> cluster management
         +-> neutral VAM input/model
    -> Vanetza VAM wrapper / ASN.1 / UPER
    -> common security and BTP / GeoNetworking runtime
    -> ns-3 access / channel
    -> common RX / security / BTP dispatch
    -> VBS receiver / semantic & plausibility validation
    -> LDM / application delivery
```

### 6.3 Reutilização obrigatória

Reutilizar, quando aplicável:

- `PacketContext` e a taxonomia global TX/RX;
- `VanetzaRuntime`;
- wrappers BTP e GeoNetworking comuns;
- segurança comum, verificação e replay;
- gestão de identidade e GN_ADDR existentes;
- access adapter / channel ns-3;
- logging e `v2x_analyzer`;
- padrões de testes Red/Green das Etapas 8–10.

Não criar `VamBtpAdapter`, `VamSecurityRuntime` ou `VamPacketContext` paralelos se os serviços partilhados suportarem a configuração VAM.

## 7. Fase 11.0 — Gate e freeze pós-CPM

### Objetivo

Registar baseline pós-Etapa 10 e impedir que alterações VAM confundam regressões anteriores.

### Ações

1. Verificar o closeout CPM no checkout atual.
2. Registar commit, branch, status e artefactos de validação.
3. Executar build de produção e suites CAM/DENM/SPATEM/CPM/analyzer.
4. Guardar logs e contagens de referência.
5. Inventariar ficheiros não rastreados e alterações locais antes de qualquer operação Git.

### Critério de saída

Baseline pós-CPM reproduzível e sem gates anteriores abertos. Caso contrário `BLOCKED`.

## 8. Fase 11.1 — Auditoria ETSI, Vanetza, ASN.1 e CDD

### Objetivo

Identificar o suporte VAM realmente disponível na instalação local e congelar uma combinação normativa/implementação coerente.

### Auditoria local

Inspecionar headers, fontes e bibliotecas efetivamente ligados:

```text
VAM / VruAwarenessMessage
VRU Basic Service / VBS
VruProfile / VruType / VRU role
VruBasicContainer / VruHighFrequencyContainer
VruLowFrequencyContainer
VruClusterInformationContainer
VruClusterOperationContainer
VruMotionPredictionContainer
ITS PDU Header / message ID / protocol version
ETSI-ITS-VAM ASN.1 module / OID / hash
CDD imports and version
UPER encoder / decoder
TS 103 097 signer / verifier
ITS-AID / VAM-SSP / BitmapSsp
BTP destination port 2018 / type B
GeoNetworking SHB / security / lifetime / traffic class
Vanetza facilities / VBS APIs, if present
```

Identificar a origem de cada header e biblioteca (`/usr/local`, build local, pacote, outra prefix path), para evitar misturar interfaces e bibliotecas de releases diferentes.

### Matriz de auditoria obrigatória

| Elemento | Evidência local | Estado |
|---|---|---|
| TS 103 300-3 | edição publicada e alterações recentes verificadas | TBD |
| TS 103 300-2 | perfil e gestão VRU compatíveis | TBD |
| VAM ASN.1 | módulo, OID, hash, origem e namespace | TBD |
| CDD | versão, imports e symbols | TBD |
| UPER encoder | header/symbol/linkage | TBD |
| UPER decoder | header/symbol/linkage | TBD |
| VBS / facilities | API existente ou decisão de implementação neutra | TBD |
| ITS-AID | 638 | TBD |
| VAM SSP | formato e versão da TS escolhida | TBD |
| Security R2 | signer/verifier e política de credenciais | TBD |
| BTP | port 2018, Type B | TBD |
| GN | SHB, SECURED, traffic class, lifetime | TBD |
| CMake | includes, libs e targets reais | TBD |

### Critério de saída

A versão VAM + CDD + codec + security + transporte está identificada, e um probe mínimo demonstra que o ASN.1 selecionado pode ser compilado e ligado sem alterar os targets de produção. Não avançar com estruturas geradas por uma versão diferente.

## 9. Fase 11.2 — Perfil, papel VRU e regras de operação

### Objetivo

Formalizar quem é a VRU, quando o dispositivo participa no serviço e que comportamento de emissão/receção fica autorizado.

### Estados de papel

O VBS deverá distinguir pelo menos:

```text
VRU_ROLE_OFF
    -> não transmite nem recebe VAM
    -> continua a monitorizar uma possível transição de papel

VRU_ROLE_ON
    -> pode transmitir e/ou receber VAM
    -> aplica perfil e estado VBS
```

`VRU_ROLE_OFF` representa contextos em que o utilizador não é tratado como VRU, por exemplo passageiro dentro de um carro/autocarro ou em área de risco nulo. A transição para `VRU_ROLE_ON` deverá resultar de uma indicação explícita da entidade de gestão do perfil/contexto, não de uma inferência arbitrária do codec.

### Estados VBS relevantes

Modelar e testar os estados necessários à primeira release:

```text
VRU-IDLE
VRU-ACTIVE-STANDALONE
VRU-ACTIVE-CLUSTER-LEADER
VRU-PASSIVE
```

As transições completas de cluster serão desenvolvidas nas fases 11.15. O Profile 3 tem restrições próprias e não deve ser tratado como um cluster VAM convencional.

### Critério de saída

Tabela de transições, eventos de entrada/saída, permissões de TX/RX por estado, políticas de perfil e testes da mudança `ROLE_OFF -> ROLE_ON -> ROLE_OFF`.

## 10. Fase 11.3 — Modelo de dados neutro e identidade

### Objetivo

Criar tipos de domínio VAM sem dependência direta de estruturas ASN.1 geradas. O modelo deverá aceitar origens ITS-S distintas sem acoplar a semântica VAM a `VehicleState`, ao identificador SUMO ou a uma única classe de estação.

### Decisão arquitetural — origens ITS-S

A fronteira de domínio deverá suportar dois tipos de origem:

```text
VamOriginContext
├── SumoPersonOrigin
│   ├── referência à entidade SUMO Person
│   ├── referência ao nó ns-3 próprio da ITS-S
│   └── fonte de mobilidade associada à Person
└── IndependentItsStationOrigin
    ├── referência lógica à ITS-S independente
    └── fonte própria de mobilidade / PoTi
```

Ambas as origens alimentam o mesmo contrato neutro `VamInput` / `VamSemanticData`. A origem identifica de onde vêm os dados e como a estação está integrada na simulação; não altera a semântica VAM nem cria um segundo pipeline de transporte, segurança ou receção.

**Primeiro cenário funcional:** `SumoPersonOrigin`, com uma SUMO Person associada a um nó ns-3 próprio e a uma ITS-S emissora, usando `VRU Profile 1 — Pedestrian`. Cada emissor representa a sua própria estação. Não se deverá criar uma estação agregadora que publique indiscriminadamente o estado de todas as pessoas SUMO.

O cenário `IndependentItsStationOrigin` fica previsto no contrato, mas não é o primeiro alvo de integração funcional.

A viabilidade da associação SUMO Person → nó ns-3 próprio → ITS-S ainda deverá ser confirmada contra a implementação real do `SumoAdapter`, a criação de nós ns-3 e o ciclo de vida dos objetos SUMO. Esta decisão de desenho não constitui evidência de que esse caminho já exista.

### Autoridade de identidade e pseudónimos

A atribuição e alteração de `stationId` deverá pertencer a uma autoridade partilhada, designada **ITS Station Identity Authority**. Esta autoridade deverá garantir unicidade entre ITS-S ativas e aplicar as regras de ciclo de vida de identidade definidas para a implementação VAM, sem delegar essa responsabilidade ao modelo semântico ou à origem SUMO.

`VehicleIdentityInitializer` deverá manter o comportamento atual do caminho de veículos e evoluir, quando necessário, para um adaptador da autoridade partilhada. A generalização não deverá provocar regressões na inicialização de identidade CAM/DENM/SPATEM/CPM.

Um **Pseudonym Lifecycle Manager** deverá coordenar referências operacionais a credenciais pseudónimas e as transições associadas. A referência ao pseudónimo é metadado operacional temporário: não pode estabelecer nem preservar uma identidade pessoal estável entre pseudónimos. O ID SUMO Person é apenas uma referência à entidade de simulação e nunca poderá ser usado como `stationId`, identidade criptográfica ou chave persistente de pessoa.

`VehicleRegistry` permanece um registo de estado e índices/reverse indexes do domínio de veículos; não deverá tornar-se autoridade de identidade nem passar a ser o registo geral de pessoas/VRUs. `StationEndpointRegistry` pode representar endpoints `ItsStation` sem `vehicleId`, mas a integração VAM terá de definir explicitamente a associação ao allocator de `stationId`, ao tipo de estação, ao perfil/papel autorizado e ao ciclo de vida de credenciais.

### Impacto nos componentes existentes

| Componente | Papel preservado / alteração prevista | Estado |
|---|---|---|
| `VehicleIdentityInitializer` | Preservar a inicialização atual de veículos; funcionar como adaptador para a autoridade partilhada quando esta for introduzida | PROPOSED |
| `ITS Station Identity Authority` | Autoridade única para atribuição/alteração de `stationId` e unicidade das ITS-S ativas | PROPOSED |
| `Pseudonym Lifecycle Manager` | Coordenar transições e referências temporárias a pseudónimos, sem identidade pessoal persistente | PROPOSED |
| `VehicleRegistry` | Manter estado e índices de veículos; não assumir gestão geral de ITS-S/VRUs | PRESERVED |
| `StationEndpointRegistry` | Reutilizar endpoint `ItsStation`; completar integração explícita com identidade, perfil/papel e credenciais VAM | PARTIAL / TO DESIGN |
| `VruRoleStateMachine` / `VruRolePolicy` | Reutilizar os tipos e políticas existentes da Etapa 11.2; não duplicar enums ou máquinas de estados | REUSE |
| `PacketContext` | Continuar como fonte de metadados de pacote, tracing e pipeline; não copiar esses campos para o modelo semântico | PRESERVED |

A `VruRoleStateMachine` e a `VruRolePolicy` não substituem autorização criptográfica. O perfil e o papel VRU são dados/políticas de facilities; a autorização do certificado e do VAM SSP pertence à camada de segurança e permanece sujeita ao gate normativo da Etapa 11.1.

### Modelo conceptual

```text
VamInput / VamMessage
├── originating station
│   ├── stationId
│   ├── originating ITS-S type
│   ├── VRU profile / sub-profile
│   ├── VRU role / VBS state
│   └── signing pseudonym reference (metadata only)
├── generation time
├── reference position / reference timestamp
├── high-frequency state
│   ├── heading
│   ├── speed
│   ├── lane / road position (when available)
│   ├── acceleration
│   ├── angular dynamics / movement indicators (when available)
│   └── device/environment data (when available)
├── low-frequency attributes (conditional)
│   ├── VRU profile / sub-profile
│   ├── size / dimensions
│   └── lights or other applicable fields
├── cluster information (optional)
├── cluster operation (optional)
└── motion prediction (optional)
```

Os campos, unidades, limites e condições de presença deverão ser derivados do ASN.1 normativo auditado. O modelo neutro não deverá inventar valores para campos que a fonte SUMO/sintética não fornece.

### Separação de identidades

```text
stationId             -> identidade ITS da estação no PDU
GN_ADDR               -> identidade de rede GeoNetworking
Link MAC              -> identidade da camada de acesso
packetId              -> identidade física / tracing do pacote
certificate pseudonym -> identidade de assinatura / credencial
VRU profile           -> classe funcional da VRU
clusterId             -> identidade lógica do cluster
SUMO person / vehicle -> ID da entidade de simulação
```

Estas identidades não são intercambiáveis. Em particular:

- não copiar o ID TraCI para `stationId` sem allocator e regras de pseudónimo;
- não usar MAC ou GN_ADDR como `stationId`;
- não usar `packetId` como identidade da pessoa;
- não preservar uma identidade estável da pessoa através de mudanças de certificado;
- validar, na auditoria normativa, as regras ETSI aplicáveis à atribuição ou alteração de `stationId` quando muda o certificado pseudónimo ou quando a VRU passa de emissão em cluster para emissão individual; até essa confirmação, estes comportamentos permanecem requisitos por validar e não devem ser implementados com base numa interpretação provisória;
- não usar o ID SUMO Person como identidade criptográfica, `stationId` ou chave persistente entre pseudónimos;
- não tratar `VehicleRegistry` como autoridade de identidade nem como registo geral de pessoas/VRUs;
- garantir que cada SUMO Person emissora tem uma ITS-S/nó ns-3 próprio no cenário inicial, sujeito à validação da integração SUMO/ns-3.

### Contratos neutros propostos

O padrão de modelação deverá seguir a separação conceptual já usada no CPM, sem copiar os seus campos específicos:

```text
VamSemanticData
├── originating station
├── VRU profile / sub-profile
├── VRU role / VBS state
├── generation time
├── reference position / timestamp
├── high-frequency state
├── conditional low-frequency attributes
├── optional cluster information / operation
└── optional motion prediction

VamInput   = VamSemanticData
VamMessage = VamSemanticData + receivedAtUnixMs
```

`VamOriginContext` é uma fronteira de integração proposta, não um tipo já existente. A origem SUMO ou independente deverá ser resolvida antes de construir `VamInput`; o modelo semântico não deverá depender de `VehicleState`, TraCI, Vanetza ou headers ASN.1. Metadados de transporte, segurança, tracing, sender/receiver e `packetId` continuam em `PacketContext` e nos contextos operacionais apropriados.

### Limitações e dependências ainda abertas

- A Etapa 11.1 mantém bloqueada a seleção normativa final do par VAM ASN.1/CDD. Não congelar campos ASN.1, unidades, limites, condições de presença, enums ou regras de codificação com base apenas neste modelo conceptual.
- Não foi demonstrada ainda uma fonte de perfil/equipamento para SUMO Person. O Profile 1 do cenário inicial deverá ser fornecido explicitamente pela configuração/fonte de domínio; não inferir perfil a partir do ID, velocidade, posição ou movimento.
- A existência de `StationEndpointRegistry::RegisterItsStation` permite representar uma estação sem `vehicleId`, mas não prova, por si só, que a criação, mobilidade, identidade, credenciais e runtime de uma ITS-S independente estejam integrados de ponta a ponta.
- A origem SUMO Person com nó ns-3 próprio requer validação específica do caminho de ingestão SUMO/TraCI e da criação/remoção dos nós.
- A mudança de pseudónimo e a transição cluster → emissão individual dependem das regras normativas confirmadas e da fase de gestão de cluster; não implementar sem fechar essas dependências.

### Critério de saída

Modelo neutro testável, matriz de identidade e testes de unicidade, mudança de pseudónimo, saída de cluster e não colisão entre identidades.

## 11. Fase 11.4 — PoTi, coordenadas e tempo

### Objetivo

Definir dados espaciais/temporais corretos para o peão emissor e para a interpretação do recetor.

### Domínios espaciais

Documentar e testar a conversão entre:

```text
SUMO / TraCI coordinates
ns-3 simulation coordinates
local ENU (if used internally)
WGS84 latitude / longitude / altitude
VAM reference position and heading
```

A posição de referência do VAM deverá respeitar o sistema geodésico e o formato do ASN.1 selecionado. Não transportar coordenadas locais como se fossem WGS84. Fixar origem, eixos, sinal, unidades, rotação, datum e quantização.

### Domínios temporais

Separar:

```text
SUMO simulation time
ns-3 simulation / scheduler time
PoTi timestamp
VAM generation time / generationDeltaTime
reference position timestamp
received VAM age / freshness
```

A referência temporal do VAM deverá corresponder ao instante em que a posição de referência foi determinada. A diferença entre a hora de geração e o timestamp de referência tem de cumprir o limite da TS selecionada (na V2.3.1, inferior a 32 767 ms). O sincronismo temporal entre ITS-S é uma dependência explícita; a especificação VAM não substitui o mecanismo de sincronização.

### Testes obrigatórios

- posições conhecidas nos quatro quadrantes e em diferentes orientações;
- conversão SUMO -> WGS84 -> VAM e retorno sem erro de eixo/sinal;
- latitude/longitude/altitude não finitas ou fora dos limites rejeitadas;
- velocidade, heading e aceleração com unidades e intervalos corretos;
- referência temporal alinhada com a posição;
- rollover do campo temporal e idade da mensagem;
- atrasos de fonte e dados stale detetados;
- nenhum timestamp gerado a partir do relógio de parede quando o cenário depende do tempo de simulação.

### Critério de saída

Conversões e timestamps cobertos por testes independentes do codec e do transporte.

## 12. Fase 11.5 — VBS generation management e triggers

### Objetivo

Implementar a política de geração VAM da VBS, separada do encoder e do transporte.

### Timers de referência — comunicação direta

| Parâmetro | Valor de referência V2.3.1 | Implementação |
|---|---:|---|
| `T_GenVamMin` | 100 ms | limite inferior configurável |
| `T_GenVamMax` | 5 000 ms | intervalo máximo configurável |
| `T_GenVamLFMin` | 2 000 ms | mínimo entre Low Frequency Containers |
| `T_CheckVamGen` | igual ou inferior a `T_GenVamMin` | periodicidade da avaliação |
| `T_AssembleVAM` | 50 ms | orçamento máximo de montagem |

Para ITS-G5, `T_GenVam` deverá respeitar a indicação de DCC e ficar limitado pelo intervalo normativo. Para PC5/LTE-V2X, deverá respeitar o mecanismo de controlo de congestionamento aplicável; não reutilizar sem justificação uma política ITS-G5.

### Triggers individuais a implementar

- primeira emissão imediatamente após ativação da VBS;
- transição para estado ativo individual;
- tempo desde a última transmissão superior a `T_GenVamMax`;
- mudança de posição de referência acima do threshold;
- mudança de velocidade acima do threshold;
- mudança de orientação da velocidade acima do threshold;
- alteração relevante da probabilidade de interceção de trajetória quando exista um modelo de trajetória válido;
- outros triggers previstos pela edição selecionada.

### Valores de referência de triggers

| Parâmetro | Valor/range indicado na V2.3.1 |
|---|---:|
| mudança de posição de referência | 4 m |
| mudança de velocidade | ±0,5 m/s |
| mudança de orientação do vetor de velocidade | ±4° |
| mudança da probabilidade de interceção de trajetória | 10 pontos percentuais |
| `numSkipVamsForRedundancyMitigation` | 2 a 10 (exemplo indicado: 4) |

Estes valores devem ficar configuráveis, identificados como valores recomendados da edição auditada e não como constantes universais. O projeto não deverá gerar uma probabilidade de colisão fictícia apenas para ativar um trigger; se não existir modelo de trajetória validado, esse trigger fica não disponível e documentado.

### Redundancy mitigation

A política de redundância deverá ser implementada em componente separado e testada contra os critérios da especificação, incluindo informação recente recebida de pares, variação de posição/velocidade/orientação, áreas protegidas e informação sobre cluster. O mecanismo não pode suprimir VAMs críticos indefinidamente nem transformar a ausência de emissão num silêncio sem motivo observável.

### Low Frequency Container

O primeiro VAM após ativação deverá incluir os dados low-frequency obrigatórios para o perfil. As emissões seguintes deverão respeitar a periodicidade mínima e as condições de cluster/alteração previstas na edição aprovada.

### Critério de saída

Testes deterministicamente temporizados para ativação, ausência de movimento, alteração de posição, velocidade, direção, LF container, DCC clamp, redundância, e limite de montagem de 50 ms.

## 13. Fase 11.6 — ASN.1 / UPER VAM codec

### Objetivo

Construir ou integrar o codec real da edição aprovada, mantendo-o dentro da fronteira Vanetza.

Local conceptual:

```text
scratch/vanetza_integration/wrappers/vanetza/vam/
```

### Estrutura lógica esperada

```text
ITS PDU Header
    protocolVersion = 3 (perfil ETSI V2.3.1)
    messageID = vam(16)
    stationId

VAM
    generationDeltaTime
    basicContainer
    vruHighFrequencyContainer
    vruLowFrequencyContainer (conditional)
    vruClusterInformationContainer (optional)
    vruClusterOperationContainer (optional)
    vruMotionPredictionContainer (optional)
```

A presença de containers deverá obedecer às regras condicionais da release. Não criar sempre todos os containers, nem omitir um container obrigatório para o caso de uso.

### Testes mínimos

```text
VamInput
    -> build ASN.1 VAM
    -> UPER encode
    -> bytes
    -> UPER decode
    -> VamMessage
    -> semantic comparison
```

Cobrir:

- fixture válida da edição escolhida;
- round-trip dos campos do perfil pedestrian inicial;
- Low Frequency Container presente/ausente conforme tempo e condições;
- valores limites de posição, velocidade, heading e tempos;
- cada container opcional quando introduzido;
- mensagens truncadas, bytes inválidos e campos obrigatórios ausentes;
- message ID/protocol version incompatíveis;
- enumerações de perfil/tipo não suportadas;
- campos ASN.1 de versões não compatíveis;
- comprimento máximo de payload e falha de montagem.

### Critério de saída

Round-trip e fixtures passam na edição escolhida; decoder rejeita payload inválido; campos semânticos essenciais não se perdem.

## 14. Fase 11.7 — Integração BTP / GeoNetworking

### Objetivo

Encaminhar VAM pelo caminho comum com os parâmetros específicos da VRU Awareness Basic Service.

### Requisitos

- BTP Type B;
- destination port 2018, validado na TS 103 248 e na configuração local;
- GeoNetworking SHB para o baseline direto;
- GN traffic class igual à classe definida para CAM no perfil em execução;
- maximum packet lifetime não superior a 1 000 ms;
- security profile `SECURED`;
- ITS-AID 638 no envelope de segurança;
- dispatch RX explícito para port 2018 / VAM;
- preservar os parâmetros de encaminhamento separados do payload ASN.1.

A implementação não deverá copiar o adapter CAM e trocar apenas a porta. Deverá reutilizar `BtpTxRequest`, wrappers GeoNetworking, access adapter e DCC existentes, representando o perfil VAM explicitamente.

### Critério de saída

Teste de integração que demonstre os parâmetros efetivamente presentes no pedido TX, a composição do pacote e o encaminhamento RX para o handler VAM correto.

## 15. Fase 11.8 — PKI, certificados e segurança VAM

### Objetivo

Garantir comunicação segura VAM desde o primeiro E2E. Segurança não é uma fase opcional posterior ao transporte.

### 15.1 Estrutura de credenciais obrigatória

A arquitetura do projeto exige uma pasta dedicada para credenciais de cada protocolo. VAM deverá possuir a sua própria subpasta:

```text
certificates/
└── VAM/
    ├── README.md
    ├── manifest/
    ├── trust-roots/
    ├── authorization-authorities/
    ├── enrolment-authorities/
    ├── authorization-tickets/
    ├── vru-profile-1/
    ├── vru-profile-2/          # quando existir caso de uso aprovado
    └── test-fixtures/
```

A estrutura final deve adaptar-se ao modelo PKI realmente usado no projeto e não presumir que todos os elementos da hierarquia estejam presentes. A primeira tarefa é inventariar `~/vanetza-pki-lab`, os diretórios de certificados já existentes, os formatos e os identificadores de credencial. Não mover, copiar ou renomear credenciais existentes sem inventário e decisão explícita.

A separação física `certificates/VAM/` é uma regra de organização do projeto. Não significa, por si só, que ETSI exija uma hierarquia de confiança raiz separada para cada protocolo. Credenciais ou autoridades comuns só podem ser referenciadas depois de validar permissões, ITS-AID, SSP, perfil e política de confiança.

### 15.2 Regras de proteção de chaves

- não colocar chaves privadas de produção em Git;
- aplicar permissões de filesystem restritas às chaves;
- manter `.gitignore` explícito para material privado e outputs temporários;
- fixtures de teste só podem ser versionadas se forem deliberadamente não produtivas, sem valor de identidade real, e aprovadas pelo projeto;
- criar manifest com finalidade, perfil, validade, AID/SSP esperado, issuer/chain e SHA-256 dos artefactos públicos;
- não registar segredos, PINs ou passwords nos logs;
- não reutilizar automaticamente os certificados DENM/CPM sem verificar permissões VAM.

### 15.3 Segurança TX

A TS 103 300-3 exige assinatura ITS, segundo TS 103 097, para VAM enviados por links sem trust services. No perfil BTP/GeoNetworking, a assinatura deverá ocorrer na camada GeoNetworking.

Validar:

- certificado e cadeia de confiança;
- validade temporal e estado de revogação conforme capacidades do ambiente;
- ITS-AID 638;
- VAM-SSP / BitmapSsp exato da edição aprovada;
- signer pseudonym e respetiva política de anexação;
- assinatura e integridade do envelope;
- coerência entre perfil do transmissor, permissões e tipo de mensagem.

A V2.3.1 indica anexação de certificado em VAM individual pelo menos uma vez por segundo ou quando observado um novo CAM signer; para VAM de cluster, pelo menos uma vez por 500 ms. Entre anexações, aplica-se `SignerIdentifier` do tipo digest, de acordo com a política normativa. Esta política deverá ser implementada e observada em testes, não substituída por anexação permanente sem análise de impacto.

### 15.4 Segurança RX

O recetor deverá:

1. localizar/validar a credencial do signer;
2. validar cadeia, validade, revogação quando disponível e permissões;
3. verificar assinatura antes de permitir a entrega à aplicação;
4. confirmar AID 638 e VAM-SSP correto;
5. aplicar freshness/replay;
6. rejeitar mensagens sem credencial válida, assinatura inválida ou permissões inadequadas;
7. nunca usar fallback legacy/inseguro para entregar VAM.

### 15.5 Privacidade e pseudónimos

A identidade VAM não pode funcionar como identificador permanente da pessoa. O plano deverá testar mudanças de pseudónimo/certificado e a consequente alteração de `stationId`, sem associar externamente a nova identidade à antiga através de logs ou métricas estáveis desnecessárias. Nos artefactos de teste, usar identidades e credenciais sintéticas.

### Critério de saída

Existe `certificates/VAM/` documentada; TX assinado; RX verifica AID/SSP, certificado e assinatura; rejeições de segurança são observáveis e não produzem `DELIVER_OK`; pseudonym rotation e certificate attachment policy são testadas.

## 16. Fase 11.9 — VAM TX no runtime

### Objetivo

Implementar transmissão VAM pelo caminho de produção e pela taxonomia global.

### Fluxo esperado

```text
VRU_ROLE_ON / VBS activation
    -> VAM trigger
    -> GENERATE
    -> ENCODE
    -> SIGN
    -> BTP_TX port=2018
    -> GEONET_TX SHB
    -> ACCESS_TX
    -> SEND
    -> TX_COMPLETE
```

### Cenário inicial

Um peão Profile 1, estacionário e depois em movimento, com fonte de dados determinística. A primeira emissão deverá ocorrer imediatamente após a ativação da VBS. Alterações conhecidas de posição/velocidade/direção deverão provocar emissões conforme a política congelada.

### Evidência mínima

```text
VAM_ROLE_ON
VAM_VBS_ACTIVATED
VAM_GENERATION_TRIGGER
VAM_GENERATE_OK
VAM_ENCODE_OK
VAM_SIGN_OK
BTP_TX port=2018 type=B
GEONET_TX transport=SHB aid=638
ACCESS_TX
SEND_OK
TX_COMPLETE RESULT=OK
```

Os nomes finais dos eventos devem adaptar-se à taxonomia global existente. Eventos auxiliares VAM não devem criar uma taxonomia concorrente.

### Critério de saída

VAM assinado transmitido pelo caminho de produção, com os campos semânticos confirmados e correlação de pacote via `PacketContext`.

## 17. Fase 11.10 — VAM RX, plausibilidade e LDM

### Objetivo

Receber VAM por port 2018, verificar segurança, decodificar e disponibilizar dados atualizados à LDM/aplicação.

### Pipeline esperado

```text
RAW_RX
    -> ACCESS_RX
    -> GEONET_RX
    -> BTP_RX port=2018
    -> VERIFY
    -> DECODE
    -> REPLAY / FRESHNESS
    -> RELEVANCE CHECK
    -> INTEGRITY CHECK
    -> PLAUSIBILITY CHECK
    -> RECEPTION
    -> LDM UPDATE
    -> DELIVER
```

A ordem interna deverá respeitar o pipeline real da infraestrutura atual, mas nenhuma mensagem não verificada, inválida ou stale poderá ser entregue.

### Validação no recetor

- `protocolVersion`, `messageID`, stationId e containers;
- limites ASN.1 e presença condicional;
- tipo de estação e perfil VRU coerentes;
- velocidade, heading, aceleração e posição plausíveis;
- consistência temporal e mensagem dentro da janela de validade;
- relevância geográfica/contextual para o recetor;
- transições individual/cluster sem estado impossível;
- atualização e expiração corretas da entrada LDM.

### LDM e freshness

O recetor deverá atualizar a entrada da VRU com base em identidade/pseudónimo, timestamp, origem e estado de cluster. Deverá definir:

- chave e ciclo de vida da entrada;
- substituição de observações recentes;
- validade máxima alinhada com lifetime e política VBS;
- expiração de estado antigo;
- comportamento perante mensagens fora de ordem;
- eliminação/transição em saída de cluster ou rotação de pseudónimo;
- diferenciação entre mensagem recebida, mensagem válida e VRU atualmente considerada relevante.

### Critério de saída

VAM RX chega à VBS, é verificado, descodificado, validado e entregue com campos coerentes. Mensagens inválidas, não autorizadas, não relevantes ou expiradas não atualizam a LDM nem geram `DELIVER_OK`.

## 18. Fase 11.11 — Replay, duplicados e ordenação

### Objetivo

Demonstrar proteção contra repetição de mensagens e gestão de informação fora de ordem.

### Requisitos

- usar o mecanismo de replay comum do projeto;
- distinguir replay criptográfico de mensagem semanticamente repetida, atraso de rede e reorder;
- não utilizar `packetId`, `stationId` isolado ou clusterId como chave universal de replay;
- associar a decisão aos dados de segurança, signer, freshness e sequência temporal que estejam disponíveis no envelope e protocolo;
- garantir que um pacote duplicado não causa uma segunda entrega/aplicação indevida;
- permitir que um VAM legítimo mais recente atualize a mesma VRU quando as regras de pseudónimo/identidade o permitem.

### Testes essenciais

```text
first valid signed VAM      -> VERIFY_OK / REPLAY_OK / DELIVER_OK
same signed packet replay   -> REPLAY_DROP
older valid VAM after newer -> stale/out-of-order policy applied
invalid signature           -> VERIFY_FAIL / no delivery
expired VAM                  -> freshness reject / no LDM update
```

### Critério de saída

Os casos de aceitação/rejeição são separados e visíveis em runtime; nenhum duplicado inválido chega à aplicação.

## 19. Fase 11.12 — PacketContext, logging e observabilidade

### Objetivo

Tornar o percurso VAM rastreável sem duplicar a observabilidade do projeto.

### Taxonomia global

TX:

```text
GENERATE -> ENCODE -> SIGN -> BTP_TX -> GEONET_TX -> ACCESS_TX -> SEND -> TX_COMPLETE
```

RX:

```text
RAW_RX -> ACCESS_RX -> GEONET_RX -> BTP_RX -> VERIFY -> DECODE
        -> REPLAY -> RECEPTION -> DELIVER
```

### Campos VAM adicionais

Quando disponíveis, registar:

```text
packetId
stationId (pseudonym-scoped)
certificate digest / signer reference (sem dados secretos)
VRU profile / originating station type
VBS role and state
reference timestamp / generation time
position / speed / heading (com precisão e política de privacidade adequada)
BTP port / type
GN transport / lifetime / traffic class
ITS-AID / security outcome / certificate attachment type
clusterId / cluster state / cardinality (quando aplicável)
encoded byte count
VAM generation trigger / suppression reason
relevance / plausibility / freshness result
LDM update / expiry / delivery result
```

Não expor identificadores pessoais estáveis. `PacketContext` continua a representar um pacote físico. Não criar um `PacketContext` por campo, por VAM attribute ou por membro de cluster.

### Critério de saída

É possível seguir uma transmissão de VAM desde o trigger até ao recetor por `PacketContext`/`packetId`, e analisar separadamente perfil, estado VBS, resultado de segurança, freshness e LDM.

## 20. Fase 11.13 — Atualização do `v2x_analyzer`

### Objetivo

Adicionar classificação, métricas e diagnóstico VAM ao analyzer existente sem quebrar CAM, DENM, SPATEM ou CPM.

### Métricas mínimas

```text
VAM generation events
VAM generated / encode success / sign success
VAM TX complete / failed
VAM physical packets received
VAM security verification failures
VAM decode failures
VAM semantic / plausibility failures
VAM replay drops
VAM stale / out-of-order rejects
VAM relevance rejects
VAM delivered to LDM / application
VAM generation interval distribution
VAM reference-time age at reception
VAM end-to-end delay (simulation domain)
VAM encoded size / security overhead
certificate attachment frequency
individual VAM vs cluster VAM counts
cluster cardinality and state transitions
VAM suppression / redundancy mitigation counts by reason
```

Mensagens, pacotes físicos, emissões VAM, entidades VRU e membros de cluster devem ser métricas distintas. Um cluster VAM não pode ser contado como N pacotes apenas porque representa N membros.

### Testes

- manter a suite atual do analyzer;
- adicionar fixtures VAM válidas e inválidas;
- testar port 2018 e AID 638;
- misturar CAM/DENM/SPATEM/CPM/VAM no mesmo log;
- verificar estágios ausentes ou fora de ordem;
- verificar que assinatura inválida, replay, stale, decode e plausibility têm contagens separadas;
- comprovar que a rotação de pseudónimo não cria uma ligação permanente de identidade no relatório;
- verificar que o número de membros de um cluster não aumenta a contagem de pacotes.

### Critério de saída

Suite atual + novos testes VAM passa e os relatórios distinguem pacote, mensagem, VRU, cluster e membro.

## 21. Fase 11.14 — VAM sintético E2E

### Objetivo

Provar o pipeline completo sem depender de SUMO ou de sensores físicos.

### Cenário mínimo

```text
VRU ITS-S A (Profile 1 / ROLE_ON)
    -> synthetic PoTi + device state
    -> VBS trigger / VAM build / UPER
    -> ITS security sign
    -> BTP / GeoNetworking
    -> ns-3 channel
    -> GeoNetworking / BTP RX
    -> verify / decode / plausibility / replay
    -> LDM / VBS delivery at ITS-S B
```

### Evidência TX

```text
VBS activation
VAM trigger reason
VAM semantic input
UPER payload length / decode view
SIGN_OK / AID=638 / SSP result
BTP_TX port=2018 type=B
GEONET_TX SHB / lifetime <= 1000 ms
ACCESS_TX / SEND / TX_COMPLETE
```

### Evidência RX

```text
RAW_RX / ACCESS_RX / GEONET_RX
BTP_RX port=2018
VERIFY_OK
VAM_DECODE_OK
RELEVANCE_OK
PLAUSIBILITY_OK
REPLAY_OK
LDM_UPDATE_OK
DELIVER_OK
```

O teste deverá confirmar que o payload recebido pelo canal é o payload efetivamente enviado e assinado; não poderá criar uma mensagem equivalente num caminho de teste paralelo.

### Critério de saída

E2E A -> ns-3 -> B demonstrado em runtime, com assinatura, receção, semântica e atualização LDM confirmadas.

## 22. Fase 11.15 — Integração SUMO como fonte de VRU

A integração TraCI só deverá começar depois do E2E sintético validado.

### Fluxo

```text
SUMO persons / controlled VRU entities
    -> identity/profile/role mapping
    -> position / time / movement extraction
    -> coordinate and temporal conversion
    -> device-data abstraction / synthetic sensor assumptions
    -> neutral VAM input
    -> VBS generation management
    -> Vanetza / ns-3 TX
```

### Limites do adaptador inicial

- usar uma pessoa SUMO identificável e controlada como VRU Profile 1;
- modelar a participação na VRU role separadamente do simples facto de existir como `person` no SUMO;
- não assumir que qualquer passageiro ou pessoa parada está sempre em `VRU_ROLE_ON`;
- mapear a pessoa SUMO para uma identidade ITS pseudónima gerida pelo projeto, não copiar TraCI ID para stationId;
- extrair apenas atributos suportados pela fonte e pelo perfil;
- identificar valores de aceleração/heading estimados ou derivados;
- usar tempo de simulação de forma consistente;
- limitar o alcance de receção/transmissão ao que o canal ns-3 realmente permite;
- declarar explicitamente que se trata de uma fonte de dados idealizada, sem GNSS, IMU ou sensor pessoal físico;
- preservar signing, BTP, GeoNetworking e RX comuns.

### Cenário SUMO inicial

Um peão aproxima-se de uma travessia, entra no contexto `VRU_ROLE_ON`, transmite VAM e altera a posição/velocidade durante a aproximação. Um veículo ITS-S recebe o VAM e atualiza a sua LDM. Não implementar atuação de travagem nem declarar uma função de prevenção de colisão validada.

### Critério de saída

Alterações controladas no estado SUMO produzem alterações VAM esperadas em runtime; coordenadas, timestamps, perfil, assinatura e LDM são coerentes.

## 23. Fase 11.16 — Clustering e Combined VRU

### Objetivo

Validar as funções VAM de cluster descritas na TS 103 300-3 e evitar que membros de cluster transmitam mensagens individuais incompatíveis com o estado coletivo.

### Baseline de cluster

Um cluster deverá ser representado como uma única entidade lógica. O cluster leader transmite cluster VAM com, pelo menos:

- cluster identifier;
- bounding box (círculo, retângulo ou polígono conforme o modelo selecionado);
- cardinalidade incluindo leader;
- perfil(s) dos membros, quando incluídos.

Os membros não líderes entram no estado passivo e não transmitem VAM individual durante a pertença válida ao cluster. Operações de entrada, saída e dissolução devem respeitar os containers e transições normativas.

### Casos de teste

```text
single VRU -> standalone individual VAM
member joins cluster -> join operation
leader -> cluster VAM with valid cluster info
member passive -> no individual VAM
member leaves -> leave operation / individual transition
leader lost -> recovery / new leader or standalone
cluster breakup -> disband indication / individual VAM
cluster composition changes -> cardinality and profile update
boundary changes -> trigger check
cluster certificate attachment policy -> 500 ms rule
cluster replay / stale / duplicate -> reject or update as specified
```

### Combined VRU

O caso combinado peão + bicicleta/dispositivo Profile 2 deverá ser considerado depois do cluster homogéneo. O procedimento de associação e eleição do leader depende do contexto/configuração e deverá ser explicitamente definido. Não introduzir Combined VRU no primeiro E2E individual.

### Critério de saída

As transições e mensagens cluster VAM são demonstradas em runtime; cardinalidade, identidade, leader, members passivos e operações de saída/ruptura mantêm estado coerente.

## 24. Fase 11.17 — Robustez, segurança negativa e congestionamento

### Objetivo

Provar que o VBS responde de forma controlada a entradas incorretas, informação velha, congestionamento, duplicados e mudanças de contexto.

### Casos mínimos

```text
ROLE_OFF / attempt to transmit
ROLE_ON / first VAM immediately
unsupported profile / invalid station type
profile-to-field inconsistency
missing mandatory container
invalid optional container combination
UPER truncated / malformed
wrong message ID / protocol version
wrong BTP port / BTP type
wrong ITS-AID (e.g. CA 36 instead of VA 638)
wrong VAM SSP / unsupported SSP version
invalid certificate chain
expired / revoked certificate (where fixture supports it)
unknown certificate digest
invalid signature
security downgrade / legacy fallback attempt
stale generation timestamp
reference timestamp inconsistency
out-of-range position / speed / heading / acceleration
implausible kinematics
replayed signed packet
out-of-order VAM
relevance-area rejection
LDM expiry
DCC minimum / maximum clamp
T_GenVamMax overdue
redundancy mitigation suppression
cluster duplicate / leader loss / split / merge
payload at and above supported size boundary
channel loss / duplication / reordering
```

Cada teste deverá distinguir:

```text
source / role failure
trigger not met / redundancy suppression
encode failure
security signing failure
transport failure
security verification failure
UPER decode failure
semantic / plausibility failure
relevance rejection
freshness / replay rejection
LDM update failure
application delivery failure
```

### Congestionamento e desempenho

Executar cenários com crescente número de VRUs equipadas e medir:

- VAM por segundo e por ITS-S;
- bytes/s incluindo overhead de segurança;
- intervalos de geração efetivos;
- percentagem de VAM suprimidos por redundância;
- atraso de montagem e envio;
- tempo de verificação e descodificação;
- taxa de receção, freshness e atualizações LDM;
- impacto nos serviços CAM/DENM/CPM existentes.

A primeira release não necessita de otimização prematura, mas deverá provar que a política não cria um flood por emissão incondicional de VAM.

### Critério de saída

Rejeições e supressões têm causas observáveis e determinísticas; nenhuma mensagem inválida atualiza a LDM; congestionamento não quebra a política VBS nem os serviços existentes.

## 25. Fase 11.18 — Regressão CAM + DENM + SPATEM + CPM

### Objetivo

Confirmar que a integração VAM não degrada os protocolos previamente aceites.

### Regressões obrigatórias

Executar build de produção, suites unitárias e analyzer, além dos cenários runtime de referência.

Quando o harness permitir, executar:

```text
CAM-only
DENM-only
SPATEM-only
CPM-only
VAM-only
CAM + VAM
DENM + VAM
CPM + VAM
SPATEM + VAM
CAM + DENM + SPATEM + CPM + VAM
```

Validar:

```text
BTP dispatch and ports
ITS-AID dispatch and security policy
message classification
stationId / GN_ADDR / MAC separation
certificate and pseudonym handling
PacketContext and packetId continuity
TX/RX event taxonomy
DCC / GeoNetworking / access behavior
LDM isolation and freshness by protocol
v2x_analyzer existing metrics and tests
production CMake target restriction
```

Se algum cenário combinado não estiver suportado pelo harness, registar a limitação e testar os handlers e caminhos afetados. Não afirmar E2E combinado que não foi executado.

### Critério de saída

Build, suites, E2E VAM e regressões anteriores passam; diferenças são explicadas e aceites tecnicamente.

## 26. Fase 11.19 — Closeout

A Etapa 11 só poderá ser marcada `CLOSED / VALIDATED` quando todos os gates obrigatórios da matriz de aceitação estiverem concluídos e as limitações remanescentes estiverem descritas.

O closeout deverá incluir:

```text
ETSI version / draft review / ASN.1 / CDD decision
profile / use-case decision record
VBS state / role / trigger implementation evidence
neutral model and identity decision
coordinate / time conversion evidence
UPER fixtures and round-trip
BTP 2018 / GN SHB / lifetime evidence
certificates/VAM structure and manifest
AID 638 / VAM SSP / signing and verification evidence
replay / freshness / plausibility evidence
VAM TX/RX runtime logs and PacketContext correlation
synthetic E2E result
SUMO pedestrian E2E result
cluster / combined-VRU results
robustness / congestion results
analyzer report and tests
CAM / DENM / SPATEM / CPM regression results
production build output
final git status / diff / commit IDs
documentation and project-state updates
```

Não declarar interoperabilidade OTA nem conformidade ETSI apenas com base em testes locais.

## 27. Matriz de aceitação

| ID | Critério | Evidência esperada | Estado inicial |
|---|---|---|---|
| VAM-01 | Gate Etapa 10 | closeout CPM formal | BLOCKED |
| VAM-02 | Baseline pós-CPM | HEAD, worktree, build e regressões registados | TBD |
| VAM-03 | Perfil normativo | TS 103 300-3/2 e revisão draft documentadas | TBD |
| VAM-04 | API Vanetza | headers/símbolos e origem local identificados | TBD |
| VAM-05 | ASN.1 / CDD | módulo, OID, hash e dependências compatíveis | TBD |
| VAM-06 | ITS-AID / BTP | AID 638 / port 2018 / Type B confirmados | TBD |
| VAM-07 | GN profile | SHB, SECURED, traffic class e lifetime validados | TBD |
| VAM-08 | Use case | UC-B2 / Profile 1 formalizados | TBD |
| VAM-09 | VRU role | ROLE_ON/OFF testados | TBD |
| VAM-10 | VBS states | individual / leader / passive modelados | TBD |
| VAM-11 | Neutral model | VamInput/VamMessage independentes de ASN.1 | TBD |
| VAM-12 | Identity | stationId/GN_ADDR/MAC/packetId/pseudonym separados | TBD |
| VAM-13 | PoTi / coordinates | SUMO/local/WGS84 e limites testados | TBD |
| VAM-14 | Time | reference timestamp / generationDeltaTime / freshness testados | TBD |
| VAM-15 | Generation management | timers, LF container e T_AssembleVAM | TBD |
| VAM-16 | Trigger policy | position/speed/orientation/timeout triggers | TBD |
| VAM-17 | Redundancy / DCC | suppression e clamp demonstrados | TBD |
| VAM-18 | Codec | UPER round-trip e fixtures válidas | TBD |
| VAM-19 | Negative decode | malformed / truncated / invalid fields rejeitados | TBD |
| VAM-20 | PKI folder | `certificates/VAM/` e manifest criados | TBD |
| VAM-21 | Security TX | assinatura VAM com AID 638 / SSP | TBD |
| VAM-22 | Security RX | certificate chain / signature / permissions validados | TBD |
| VAM-23 | Certificate policy | attachment / digest individual e cluster testados | TBD |
| VAM-24 | Privacy | pseudonym rotation e stationId update testados | TBD |
| VAM-25 | VAM TX | TX production path completo | TBD |
| VAM-26 | VAM RX | verify/decode/plausibility/relevance/LDM | TBD |
| VAM-27 | Replay/freshness | replay, stale e out-of-order demonstrados | TBD |
| VAM-28 | PacketContext/logging | correlação e taxonomia preservadas | TBD |
| VAM-29 | Analyzer | métricas VAM sem regressões | TBD |
| VAM-30 | Synthetic E2E | VRU A -> ns-3 -> ITS-S B | TBD |
| VAM-31 | SUMO | UC-B2 pedestrian E2E | TBD |
| VAM-32 | Cluster | join/leave/leader/breakup/cardinality | TBD |
| VAM-33 | Robustness | matriz Red/Green e razões de rejeição | TBD |
| VAM-34 | Congestion | frequência, overhead e impacto medidos | TBD |
| VAM-35 | CAM regression | baseline CAM preservado | TBD |
| VAM-36 | DENM regression | baseline DENM preservado | TBD |
| VAM-37 | SPATEM regression | baseline SPATEM preservado | TBD |
| VAM-38 | CPM regression | baseline CPM preservado | TBD |
| VAM-39 | Production build | target de produção PASS | TBD |
| VAM-40 | Runtime evidence | logs, contagens e resultados reproduzíveis | TBD |
| VAM-41 | Documentation | plano, estado, comandos e evidência atualizados | TBD |
| VAM-42 | Closeout | todos os gates obrigatórios aceites | TBD |

## 28. Evidência mínima por fase

Guardar, conforme aplicável:

```text
comandos executados e exit codes
commit / branch / git status / diff
ETSIs e drafts consultados com revisão/data
ASN.1 VAM, OID, hash e origem do CDD
headers, symbols e bibliotecas locais
fixtures válidas e malformadas
resultados RED e GREEN
build e link output
VAM payload size / security overhead
VBS role/state transition logs
trigger reason / generation interval / DCC values
PacketContext / packetId TX-RX correlation
AID / SSP / signer / verify outcome (sem segredos)
replay / stale / plausibility / relevance results
LDM add/update/expire evidence
SUMO source-to-VAM semantic comparison
cluster transition and cardinality evidence
analyzer output
regressions CAM / DENM / SPATEM / CPM
hashes dos artefactos de evidência, quando usados
```

A evidência tem de separar:

```text
SOURCE EVIDENCE  -> a implementação corresponde ao requisito
BUILD EVIDENCE   -> o código compila e liga
RUNTIME EVIDENCE -> o comportamento ocorreu e foi observado
```

Para cada `PASS`, guardar comando/teste, resultado, artefacto e critério verificado. Não reutilizar logs de versões anteriores como prova de uma build atual.

## 29. Organização conceptual dos artefactos

A estrutura final deverá seguir os padrões atuais do repositório, sem duplicar serviços comuns:

```text
scratch/vanetza_integration/
├── vam/                         # domínio VBS, estado, trigger, receive
├── wrappers/
│   └── vanetza/
│       └── vam/                 # ASN.1 builder / converter / codec
├── context/                     # PacketContext partilhado
├── rx/                          # pipeline comum / port dispatch
├── security/                    # security partilhada e políticas VAM
├── vanetza_runtime/             # orquestração existente
├── sumo_adapter/                # adaptação VRU/TraCI
└── ...

scratch/test/
├── Green/VAM/
└── Red/VAM/

certificates/
└── VAM/
    ├── README.md
    ├── manifest/
    ├── trust-roots/
    ├── authorization-authorities/
    ├── enrolment-authorities/
    ├── authorization-tickets/
    ├── vru-profile-1/
    ├── vru-profile-2/
    └── test-fixtures/
```

Os nomes dos diretórios PKI são conceptuais e devem ser ajustados ao modelo real de certificados utilizado. Não mover ou duplicar os certificados atuais de `~/vanetza-pki-lab` nesta etapa sem inventário, autorização e plano de migração específico.

## 30. Estratégia de commits

Cada gate aceite deverá resultar num commit separado e limitado a ficheiros intencionais. Exemplos indicativos:

```text
docs(vam): add VAM implementation and validation plan
test(vam): capture post-cpm baseline
docs(vam): record selected ETSI and ASN.1 profile
test(vam): add VRU role and identity contract
feat(vam): add neutral VAM domain model
test(vam): validate PoTi coordinate and time mapping
feat(vam): add VBS state and generation management
test(vam): add VAM UPER fixtures and malformed cases
feat(vam): integrate Vanetza VAM codec
test(vam): verify BTP 2018 and GeoNetworking profile
feat(vam): provision protocol-specific security fixtures
test(vam): verify VAM signing and authorization policy
feat(vam): add secure VAM transmit path
feat(vam): add VAM receive and plausibility pipeline
test(vam): add replay freshness and LDM expiry cases
feat(vam): add VAM analyzer support
test(vam): demonstrate synthetic end-to-end delivery
feat(vam): integrate SUMO pedestrian source
feat(vam): add VRU cluster lifecycle
 test(vam): cover congestion and protocol regressions
docs(vam): close out validation evidence
```

Os títulos são exemplos. Cada commit deve refletir apenas as alterações efetivamente validadas. Não usar `git add -A` ou `git add .`; preservar artefactos não rastreados e alterações de outros protocolos.

## 31. Riscos técnicos e mitigação

### 31.1 Versão normativa / draft em mudança

**Risco:** implementar rascunhos ou módulos ASN.1 de release diferente da TS publicada.

**Mitigação:** 11.1 fixa edição, CDD, OID, hash e estado do draft antes do código.

### 31.2 Tratar VAM como simples mensagem periódica

**Risco:** omitir a VBS, role, triggers, clustering e redundancy mitigation.

**Mitigação:** implementar o serviço e a máquina de estados antes de declarar VAM funcional.

### 31.3 Confundir perfis VRU e CAM

**Risco:** gerar VAM de motociclo ou e-scooter com semântica não suportada pela versão normativa.

**Mitigação:** Profile 1 inicial; decisões separadas para Profile 2/3/4 e revisão explícita do CAM Release 2.

### 31.4 Configurar porta/AID incorretos

**Risco:** reutilizar BTP 2001/2002 ou AID 36/37/639.

**Mitigação:** port 2018 e AID 638 confirmados na documentação, com verificação local e testes de dispatch.

### 31.5 Credenciais erradas ou ausência de segurança

**Risco:** assinar com credenciais DENM/CAM/CPM sem autorização VAM ou permitir fallback inseguro.

**Mitigação:** `certificates/VAM/`, AID/SSP explícitos, signing e verification obrigatórios antes de E2E.

### 31.6 Privacidade e rastreabilidade excessiva

**Risco:** usar IDs SUMO estáveis ou logs para seguir uma pessoa através de pseudónimos.

**Mitigação:** pseudonym rotation, alteração de stationId e minimização de dados nos logs/analyzer.

### 31.7 Posição e tempo incorretos

**Risco:** VAM UPER válido com localização ou timestamp falso.

**Mitigação:** testes PoTi independentes e freshness na receção.

### 31.8 Informação não plausível / falsos positivos

**Risco:** aplicações superiores considerarem um aviso como verdade absoluta.

**Mitigação:** plausibility check, confiança e contexto; não converter o VAM diretamente em atuação de veículo.

### 31.9 Dados SUMO omniscientes

**Risco:** transmitir todos os peões visíveis no estado global como se fossem perceção real de uma VRU.

**Mitigação:** fonte por dispositivo, role/profile explicitados e limites de perceção documentados.

### 31.10 VAM stale na LDM

**Risco:** o recetor continuar a considerar presente uma pessoa que já saiu ou cujo dispositivo deixou de transmitir.

**Mitigação:** timestamp, lifetime, timeout e expiração LDM testados.

### 31.11 Clustering inconsistente

**Risco:** leader e members transmitirem simultaneamente estados contraditórios ou perderem a cardinalidade.

**Mitigação:** máquina de estados, join/leave/breakup e leader loss testados separadamente.

### 31.12 Congestionamento e redundância

**Risco:** VAM em periodicidade fixa causar excesso de tráfego, ou a supressão ocultar mudanças relevantes.

**Mitigação:** DCC, triggers, intervalos normativos e métricas de supressão/overhead.

### 31.13 Regressões nos serviços existentes

**Risco:** alterações a BTP, GN, security, identity ou RX dispatch quebrarem CAM/DENM/SPATEM/CPM.

**Mitigação:** baseline pós-CPM e regressão obrigatória 11.18.

## 32. Fora do âmbito do primeiro baseline

```text
smartphone / GNSS / IMU / wearable hardware real
sensores físicos e fusão de sensores de produção
ITS-G5 / PC5 com hardware OTA
interoperabilidade com stacks externos
certificação formal de conformidade ETSI
travagem automática ou controlo do veículo
serviços de backend / cloud / long-range communication profile
originação VAM por VRUs não equipadas
VAM Profile 3 para motociclos contrariando a regra publicada
suporte integral de todos os perfis e containers opcionais no primeiro E2E
Combined VRU antes de cluster individual validado
```

Uma funcionalidade fora deste baseline poderá ser planeada como extensão, mas não deverá bloquear a prova do primeiro percurso VAM seguro. Os elementos que a especificação exige para o perfil declarado não podem ser excluídos apenas por conveniência.

## 33. Definição de DONE

VAM `DONE` significa que o projeto demonstra em runtime um serviço VBS coerente com a edição ETSI declarada:

```text
VRU role/profile activation
    -> PoTi / device state
    -> VBS state and generation management
    -> valid VAM ASN.1 / UPER
    -> security signing (ITS-AID 638 / VAM SSP)
    -> BTP-B port 2018
    -> GeoNetworking SHB / ns-3 channel
    -> GeoNetworking / BTP RX
    -> security verification
    -> VAM decode
    -> relevance / integrity / plausibility
    -> replay / freshness
    -> LDM update / delivery
```

com:

```text
Profile 1 pedestrian synthetic E2E PASS
SUMO pedestrian use case PASS
VBS ROLE_ON / ROLE_OFF demonstrated
VAM triggering and DCC behavior demonstrated
certificate folder certificates/VAM/ and manifest complete
AID / SSP / signature verification demonstrated
privacy and pseudonym change tested
LDM freshness and expiry validated
cluster lifecycle demonstrated
PacketContext / TX-RX taxonomy preserved
v2x_analyzer VAM metrics validated
Red / Green robustness tests PASS
CAM / DENM / SPATEM / CPM regressions PASS
production build PASS
evidence and documentation complete
```

O estado `DONE` não implica, por si só, interoperabilidade com equipamentos externos nem certificação ETSI. Esses resultados exigem validação própria, com stacks/equipamentos independentes e critérios específicos.

## 34. Próximo passo após aprovação

Depois da aprovação deste plano, o próximo passo é **11.0 — Gate e freeze pós-CPM**. Só depois de confirmar `Etapa 10 = CLOSED / VALIDATED` se deverá avançar para **11.1 — Auditoria ETSI, Vanetza, ASN.1 e CDD**.

Nenhum código VAM deverá ser criado antes de confirmar a versão ASN.1/UPER, o perfil VRU, o BTP port, o AID/SSP e a disponibilidade das APIs Vanetza.

## 35. Referências técnicas

### Normativas ETSI

- [ETSI TS 103 300-3 V2.3.1 (2025-12) — VRU Awareness Service](https://www.etsi.org/deliver/etsi_ts/103300_103399/10330003/02.03.01_60/ts_10330003v020301p.pdf)
- [ETSI TS 103 300-2 — VRU awareness requirements and architecture](https://www.etsi.org/deliver/etsi_ts/103300_103399/10330002/)
- [ETSI TR 103 300-1 — VRU use cases](https://www.etsi.org/deliver/etsi_tr/103300_103399/10330001/)
- [ETSI TS 103 248 V2.5.1 (2026-01) — BTP port numbers](https://www.etsi.org/deliver/etsi_ts/103200_103299/103248/02.05.01_60/ts_103248v020501p.pdf)
- [ETSI TS 102 965 V2.2.1 (2024-07) — ITS-AID registration](https://www.etsi.org/deliver/etsi_ts/102900_102999/102965/02.02.01_60/ts_102965v020201p.pdf)
- [ETSI TS 103 097 — ITS security headers and certificate formats](https://www.etsi.org/deliver/etsi_ts/103000_103099/103097/)
- [ETSI TS 103 836-5-1 — Basic Transport Protocol](https://www.etsi.org/deliver/etsi_ts/103800_103899/1038360501/)
- [ETSI work item RTS/ITS-0011009 — VAM update draft status](https://portal.etsi.org/webapp/WorkProgram/Report_WorkItem.asp?WKI_ID=75431)

### Projeto e implementação

- `ETAPA_8_DENM_PLANO.md` — estrutura e gates DENM.
- `ETAPA_10_CPM_PLANO.md` — metodologia, evidência, coordenadas, security/replay e validação CPM.
- `scratch/vanetza_integration/docs/vanetza_architecture.md` — confirmar caminho/nome no checkout atual.
- `scratch/vanetza_integration/docs/CURRENT_STATE.md` — confirmar estado no checkout após Etapa 10.
- Headers, bibliotecas e módulos ASN.1 realmente instalados em `/usr/local/include/vanetza` e `/usr/local/lib`.

---

**Estado do documento:** PROPOSED — sujeito a aprovação e ao gate de encerramento da Etapa 10
**Próxima fase autorizada após aprovação e gate:** 11.0 — Freeze pós-CPM
