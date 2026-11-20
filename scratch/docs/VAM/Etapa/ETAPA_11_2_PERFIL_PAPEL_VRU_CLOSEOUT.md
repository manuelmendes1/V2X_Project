---
title: "Etapa 11.2 — Perfil, Papel VRU e Regras de Operação"
status: "CLOSED / VALIDATED — ISOLATED DOMAIN SCOPE"
type: "PHASE CLOSEOUT"
date: 2026-11-04
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
stage: "ETAPA 11.2"
scope: "VRU role state machine and VBS TX/RX policy"
branch: "feature/vam-etapa11-2-vru-role"
baseline: "19b0b41b24bd561f5a23bcac9a18bcf9f9525681"
related_plan: "[[ETAPA_11_VAM_PLANO]]"
---

# Etapa 11.2 — Perfil, Papel VRU e Regras de Operação

## 1. Resultado

A Etapa 11.2 está concluída no âmbito delimitado de modelação e validação isolada de:

- estados de papel VRU;
- estados VBS relevantes;
- transições permitidas;
- permissões de transmissão e receção;
- política de emissão por perfil e confirmação de equipamento.

A implementação não está integrada no runtime VAM, no `VehicleState`, no SUMO ou nos pipelines TX/RX. Este closeout não declara VBS funcional nem transmissão/receção VAM demonstrada.

**Estado:** `CLOSED / VALIDATED — ISOLATED DOMAIN SCOPE`.

## 2. Implementação

Estrutura criada:

```text
scratch/vanetza_integration/vru/role/
├── types/
│   └── vru_role_types.h
├── state_machine/
│   ├── vru_role_state_machine.h
│   └── vru_role_state_machine.cc
└── policy/
    ├── vru_role_policy.h
    └── vru_role_policy.cc
```

Namespace:
vanetza_integration::vru::role


Tipos principais:
VruRoleStatus
VbsState
VruProfile
VruTransitionResult
VruRoleSnapshot
VruRolePermissions
VruTxDecisionReason
VruRxDecisionReason


A implementação está dividida em três fronteiras:
1. types: tipos neutros de domínio;
2. state_machine: transições do papel e estado VBS;
3. policy: avaliação independente das permissões TX/RX.
A máquina de estados e a policy não dependem diretamente de SUMO, VehicleState, VehicleRegistry, ns-3 ou Vanetza.

## 3. Contrato de estados

### 3.1 Papel VRU
| Estado | Significado | TX VAM | RX VAM |
| --- | --- | --- | --- |
| Off | Dispositivo fora do papel VRU | Não | Não |
| On | Dispositivo participa no serviço VRU | Depende do VBS e perfil | Sim |


Off mantém a possibilidade de uma futura transição explícita para On. A máquina de estados não infere o papel a partir de posição, velocidade, identificador SUMO ou conteúdo do codec.

A ativação é uma operação explícita da interface de domínio.

### 3.2 Estados VBS
| Estado | Significado operacional |
| --- | --- |
| Idle | Papel VRU ativo, sem emissão individual ativa |
| ActiveStandalone | VRU ativa individualmente |
| ActiveClusterLeader | Estado de líder de cluster; emissão adiada nesta fase |
| Passive | Estado passivo; não emite VAM individual |


As transições completas de cluster permanecem reservadas para a fase 11.15.

## 4. Contrato de transições
Operações disponibilizadas:
| Operação | Pré-condição | Resultado |
| --- | --- | --- |
| activate() | Papel Off | Passa para On / Idle |
| activate() | Papel já On | NoChange |
| deactivate() | Papel On | Passa para Off / Idle |
| deactivate() | Já Off / Idle | NoChange |
| setVbsState(Idle) | Papel Off | NoChange |
| setVbsState(non-Idle) | Papel Off | Rejected |
| setVbsState(state) | Papel On | Atualiza o VBS ou devolve NoChange |


Invariantes:
- O estado inicial é Off / Idle.
- A ativação não escolhe automaticamente um estado ativo de emissão.
- O VBS só pode sair de Idle quando o papel está On.
- A desativação repõe sempre Idle.
- Ativações e desativações repetidas são idempotentes.
- Uma transição rejeitada preserva o estado anterior.

### 4.1 Eventos de entrada e resultados
O contrato de entrada é representado pelas operações públicas da máquina:
| Entrada de domínio | Operação | Resultado observável |
| --- | --- | --- |
| Pedido explícito de ativação VRU | activate() | Applied ou NoChange |
| Pedido explícito de desativação VRU | deactivate() | Applied ou NoChange |
| Pedido de mudança do estado VBS | setVbsState() | Applied, NoChange ou Rejected |


O resultado é acompanhado pelo estado observável através de VruRoleSnapshot.

Estes são contratos de operação da máquina de estados. Não representam ainda eventos publicados num event bus ou notificações provenientes de SUMO, sensores ou de um gestor de contexto.

## 5. Política de permissões TX/RX

### 5.1 Matriz por estado
| Papel | Estado VBS | TX | RX |
| --- | --- | --- | --- |
| Off | Qualquer estado recebido no snapshot | Não | Não |
| On | Idle | Não | Sim |
| On | ActiveStandalone | Conforme perfil | Sim |
| On | ActiveClusterLeader | Não — adiado | Sim |
| On | Passive | Não | Sim |


A policy dá precedência a Off, inclusive perante um snapshot inconsistente que combine Off com um estado VBS ativo.

### 5.2 Política por perfil em ActiveStandalone
| Perfil | Confirmação de equipamento | TX | RX |
| --- | --- | --- | --- |
| Pedestrian | Não aplicável | Sim | Sim |
| BicyclistLightVruVehicle | Não aplicável | Sim* | Sim |
| Motorcyclist | Não aplicável | Não | Sim |
| Animal | Não | Não | Sim |
| Animal | Sim | Sim | Sim |
| Unknown | Não aplicável | Não | Sim |


* A permissão de Profile 2 é uma decisão provisória da policy implementada. Não significa que todos os dispositivos abrangidos por Profile 2 devam transmitir VAM no mesmo modo. A aplicabilidade concreta deverá ser confirmada contra as regras normativas e a decisão de perfil das fases 11.1 e seguintes.

A política para Profile 3 mantém a emissão VAM bloqueada, em linha com a orientação do plano para a edição publicada. Profile 4 exige confirmação explícita de equipamento originador; a mera deteção de um animal não autoriza VAM TX.

A policy permite RX em todos os estados VBS quando o papel está On. Esta é a decisão de baseline desta fase, não uma declaração de conformidade normativa independente.

## 6. Testes e evidência

### 6.1 State machine
Ficheiros:
scratch/test/Red/VAM/test_vru_role_state_machine.py
scratch/test/Red/VAM/test_vru_role_state_machine.cc


Cobertura demonstrada:
- estado inicial Off / Idle;
- ativação e idempotência;
- passagem pelos quatro estados VBS com papel On;
- preservação do papel durante mudanças VBS;
- desativação a partir de cada estado VBS;
- reposição de Off / Idle;
- rejeição de estados VBS não-Idle enquanto Off;
- preservação do estado após transição rejeitada.
Resultado observado:
VRU_ROLE_STATE_MACHINE_BUILD=PASS
VRU_ROLE_STATE_MACHINE=PASS
Ran 1 test
OK
0 falha(s), 0 erro(s)


### 6.2 Policy
Ficheiros:
scratch/test/Red/VAM/test_vru_role_policy.py
scratch/test/Red/VAM/test_vru_role_policy.cc


Cobertura:
- precedência de Off;
- Idle, ActiveStandalone, ActiveClusterLeader e Passive;
- cinco categorias de perfil;
- animal com e sem confirmação de equipamento;
- permissões e razões de decisão TX/RX.
Matriz exaustiva da combinação dos inputs definidos:
2 estados de papel
x 4 estados VBS
x 5 perfis
x 2 valores de confirmação de equipamento
= 80 casos


Resultado observado:
VRU_ROLE_POLICY_BUILD=PASS
VRU_ROLE_POLICY=PASS
VRU_ROLE_POLICY_MATRIX_CASES=80
Ran 1 test
OK
0 falha(s), 0 erro(s)


A matriz valida a implementação contra a tabela de decisão codificada no fixture. Não constitui, por si só, prova de conformidade ETSI.

### 6.3 Isolamento arquitetural
Verificação executada sobre:
scratch/vanetza_integration/vru/role/


Padrões procurados:
VehicleState
VehicleRegistry
SumoAdapter
TraCI
vanetza::
ns3::
includes diretos ns-3 / Vanetza


Resultado:
VRU_ROLE_EXTERNAL_DEPENDENCY=NONE


### 6.4 Limite de build
Verificação:
git diff --exit-code -- scratch/CMakeLists.txt
SCRATCH_CMAKE_UNCHANGED=PASS


Não foram adicionados targets de diagnóstico ao build de produção.

Os fixtures C++ são compilados pelo harness Python de testes, separadamente dos targets de produção.

## 7. Limites explícitos deste closeout
Não demonstrado nesta fase:
- integração com VehicleState ou VehicleRegistry;
- identificação de pessoas/VRUs a partir de SUMO;
- integração com um gestor de perfil/contexto;
- integração com PoTi ou Device Data Provider;
- geração, triggers ou controlo de frequência VAM;
- construção ASN.1 ou UPER;
- assinatura, autorização ITS-AID/SSP ou certificados VAM;
- BTP port 2018 ou GeoNetworking SHB;
- VAM TX/RX em runtime;
- replay, freshness, plausibility ou LDM;
- ciclo de vida de clusters;
- analyzer VAM ou cenários E2E.
A ausência destes elementos é intencional: pertencem a fases posteriores do plano e não deve ser ocultada por este closeout.

## 8. Decisões a preservar nas fases seguintes
1. A ativação VRU deve resultar de indicação explícita de gestão de papel/contexto.
2. Nenhum ID SUMO deve ser convertido implicitamente em stationId.
3. stationId, GN_ADDR, MAC, packetId, pseudónimo de certificado, perfil, clusterId e ID SUMO permanecem identidades distintas.
4. ActiveClusterLeader não autoriza TX nesta fase; a política deverá ser revista quando o ciclo de cluster for implementado na fase 11.15.
5. A política Profile 2 requer confirmação normativa e de subperfil antes de ser usada como autorização final de emissão.
6. Profile 3 permanece bloqueado para VAM TX no baseline.
7. Profile 4 requer prova de que existe uma ITS-S equipada no animal.
8. A policy não substitui a autorização criptográfica VAM nem a validação de segurança.

## 9. Estado Git observado
No momento da validação:
branch: feature/vam-etapa11-2-vru-role
HEAD:   19b0b41b24bd561f5a23bcac9a18bcf9f9525681
base:   origin/main


Ficheiros novos observados:
?? scratch/test/Red/VAM/
?? scratch/vanetza_integration/vru/


O documento de closeout é acrescentado em:
scratch/docs/VAM/ETAPA_11_2_PERFIL_PAPEL_VRU_CLOSEOUT.md


Não foi criado commit nesta fase. Os ficheiros continuam por adicionar ao índice Git até revisão e decisão explícita de commit.

## 10. Decisão de fecho
Critérios específicos da fase 11.2:
| Critério | Resultado |
| --- | --- |
| Estados de papel definidos | PASS |
| Estados VBS definidos | PASS |
| Contrato de transições | PASS |
| Operações/resultados de entrada e saída documentados | PASS |
| Permissões TX/RX por estado | PASS |
| Política por perfil | PASS, com ressalva Profile 2 |
| ROLE_OFF -> ROLE_ON -> ROLE_OFF testado | PASS |
| Testes unitários da máquina de estados | PASS |
| Testes da policy | PASS |
| Matriz de 80 combinações | PASS |
| Isolamento de dependências | PASS |
| Targets de produção preservados | PASS |


Decisão: ETAPA 11.2 = CLOSED / VALIDATED — ISOLATED DOMAIN SCOPE.

A próxima fase deverá consumir este contrato sem introduzir inferências implícitas de identidade ou papel. A integração com fontes de contexto e com o modelo neutro VAM deverá respeitar as fronteiras estabelecidas neste documento.
