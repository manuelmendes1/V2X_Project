---
title: "Etapa 11.6.1b — VRU Subject Role"
stage: "11.6.1b"
status: validated
validated_on: 2026-11-10
tags:
  - V2X
  - ns-3
  - SUMO
  - VAM
  - VRU
  - validation
---

# Etapa 11.6.1b — VRU Subject Role

## Objetivo

Associar o ciclo de vida de cada pessoa proveniente do SUMO a um sujeito
VRU independente, com perfil e estado de papel explícito.

A etapa estabelece a infraestrutura de gestão do sujeito VRU. Não demonstra,
por si só, a transmissão de mensagens VAM.

## Arquitetura e responsabilidades

### Fonte autoritativa

`SumoPersonRegistry` continua a ser responsável por:

- Identidade da pessoa SUMO.
- Amostra de mobilidade.
- Ordenação temporal dos eventos.
- Rejeição de eventos duplicados ou obsoletos.
- Tombstones e prevenção de ressurreição por eventos antigos.

### Estado derivado do sujeito VRU

`SumoVruSubjectRegistry` mantém, por pessoa:

- `VruProfile`.
- Máquina de estados `VruRoleStateMachine`.
- Estado de papel VRU.
- Estado VBS.

O registo não duplica dados de mobilidade nem a ordenação temporal da fonte.

### Sincronização

`SumoVruSubjectSynchronizer` sincroniza apenas eventos aceites pelo
`SumoPersonRegistry`.

- `UPSERT`: resolve o perfil a partir do vClass e atualiza o sujeito.
- `REMOVE`: remove o sujeito e o estado de papel associado.
- Evento rejeitado: não atualiza o registo derivado.

Um `UPSERT` de uma pessoa existente atualiza o perfil sem reinicializar
o estado de papel ou VBS.

Uma remoção seguida de uma reentrada válida cria um sujeito novo em
`OFF/IDLE`.

### Gestão explícita

O `SimulationController` expõe:

- `GetSumoVruSubjectRegistry()`
- `ActivateVruSubject(personId)`
- `DeactivateVruSubject(personId)`
- `SetVruSubjectVbsState(personId, state)`

A ativação do papel e as transições VBS são explícitas. Não são inferidas
automaticamente a partir de identidade, classe SUMO, posição ou velocidade.

Os registos Person e VRU são limpos no início da simulação.

## Perfil SUMO

O mapeamento utilizado nesta etapa reconhece `pedestrian` como
`VruProfile::Pedestrian`.

Classes SUMO não aprovadas e vClass ausente permanecem `Unknown`.
A classificação de origem é preservada como metadado e não concede,
por si só, autorização para transmissão VAM.

## Evidência de validação

### Testes automatizados

| Validação | Resultado |
|---|---|
| Regressão VAM / VRU / SUMO / POTI | 17 ficheiros de teste — PASS |
| Bridge Person | 12 testes — PASS |
| Bridge Person vClass | 4 testes — PASS |
| Compilação do target `scratch_v2x_sumo` | PASS |
| `git diff --check` — feature | PASS |
| `git diff --check` — bridge | PASS |

A regressão inclui os contratos do parser temporal ITS, Person datagram,
Person registry, perfil vClass, sincronização, estado do sujeito, política
VRU, máquina de estados e contrato POTI.

A política VRU passou a matriz de 80 casos.

### Probe UDP direto

Foram enviados `UPSERT` e `REMOVE` sintéticos para UDP 7400.

Resultados observados:

- `UPSERT`: `active_count=1`, `sumo_vclass=pedestrian`,
  `vru_profile=Pedestrian`.
- `REMOVE`: `active_count=0`, `sumo_vclass=none`,
  `vru_profile=NotPresent`.
- Processo ns-3 terminou com exit code 0.

Este probe valida o percurso UDP e o audit, mas não substitui o ensaio
com uma pessoa real do SUMO.

### E2E com pessoa real do SUMO

A fixture temporária usou:

- Pessoa: `person-pedestrian`.
- `vClass`: `pedestrian`.
- Edge: `185322867#0`.
- Posição inicial: 1,0 m.
- Posição final: 3,0 m.
- Velocidade: 1,39 m/s.

A fixture curta foi criada em `/tmp`; a fixture original não foi alterada.

Resultados observados:

| Evidência | Resultado |
|---|---:|
| UPSERT da pessoa real | 16 |
| REMOVE da mesma pessoa | 1 |
| Perfil durante a presença | `Pedestrian` |
| Perfil após remoção | `NotPresent` |
| TLS publicados pelo bridge | 240 |
| Exit code bridge | 0 |
| Exit code ns-3 | 0 |
| Exceções TraCI | Nenhuma |

A sequência temporal observada foi de 100 ms a 1.600 ms para os UPSERT,
seguida do REMOVE aos 1.700 ms.

### Audit de runtime

O audit é opt-in através de:

`V2X_SUMO_PERSON_AUDIT=1`

A emissão utiliza `std::cout`, não `NS_LOG_UNCOND`. Esta escolha é necessária
porque, na configuração de compilação utilizada, `NS_LOG_UNCOND` é eliminado
quando `NS3_LOG_ENABLE` não está definido.

Os registos incluem evento, pessoa, timestamp, contagem ativa, vClass
de origem e perfil resolvido.

## Correção independente no bridge

Durante a validação E2E foi identificada uma utilização incorreta da API
TraCI: a classe da pessoa era consultada através de
`traci.vehicletype.getVehicleClass(person_id)`.

A consulta foi corrigida para:

`traci.person.getVehicleClass(person_id)`

A alteração está separada da implementação C++ VRU e pertence à branch
`feature/vam-etapa11-3-neutral-contract`, acompanhada pelos testes
`test_v2x_bridge_16_person.py` e
`test_v2x_bridge_16_person_vclass.py`.

O E2E foi executado com a combinação das alterações não commitadas dos
dois worktrees. Consequentemente, os resultados E2E validam essa combinação,
não apenas a branch 11.6.1b isoladamente.

## Limites e trabalho ainda não demonstrado

Esta etapa valida:

- Ingestão de pessoas SUMO.
- Resolução e atualização do perfil VRU.
- Gestão explícita do papel e estado VBS.
- Sincronização e remoção do sujeito.
- Preservação e reentrada do estado.
- Continuidade da publicação TLS.

Não valida ainda o pipeline VAM completo, nomeadamente geração, codificação,
assinatura, transmissão, receção, verificação e entrega de uma mensagem VAM.

A autorização de TX/RX deverá continuar subordinada à política VRU e às
condições do estado de papel, perfil e equipamento.

## Referências de evidência local

Os logs de runtime desta validação foram preservados em:

- Probe UDP: `/tmp/vam-vru-udp-probe.0mHofS.log`
- E2E ns-3: `/tmp/vam-vru-short-walk-e2e.i6iZ2I/ns3-vru-e2e.log`
- E2E bridge: `/tmp/vam-vru-short-walk-e2e.i6iZ2I/sumo-bridge-e2e.log`

Os ficheiros temporários não são dependências da implementação nem devem
ser confundidos com artefactos versionados no repositório.

## Estado da etapa

**Etapa 11.6.1b — VRU Subject Role: VALIDADA.**

A validação funcional e a regressão estão concluídas. O pipeline VAM
completo permanece fora do âmbito desta etapa.
