---
title: "Etapa 10.3 — Modelo de dados neutro CPM"
status: "PASS_WITH_SCOPE_LIMITS"
tags: [V2X, CPM, neutral-model, validation, testing]
---

# Etapa 10.3 — Modelo de dados neutro CPM

**Estado:** PASS_WITH_SCOPE_LIMITS — aceitação limitada ao contrato do modelo neutro
**Data da evidência:** 2026-10-21
**Perfil de referência:** ETSI TS 103 324 V2.1.1, Release 2
**Branch:** `feature/cpm-etapa10-3-neutral-model`
**Commit-base registado:** `efa58c1` — `docs(cpm): accept etapa 10.2 gate`

## 1. Objetivo e âmbito

Esta fase estabelece tipos de domínio CPM independentes da representação ASN.1 gerada, juntamente com contratos de entrada, mensagem e validadores semânticos elementares.

A aceitação cobre o contrato do modelo neutro e as provas isoladas executadas. Não implica que o CPM esteja integrado no pipeline de produção ou que a Etapa 10 esteja concluída.

## 2. Artefactos abrangidos

Modelo neutro:

- `scratch/vanetza_integration/cpm/types/cpm_domain_types.h`
- `scratch/vanetza_integration/cpm/types/cpm_input.h`
- `scratch/vanetza_integration/cpm/types/cpm_message.h`

Validadores:

- `scratch/vanetza_integration/cpm/validation/cpm_management_metadata_validation.h`
- `scratch/vanetza_integration/cpm/validation/cpm_confidence_ratio_validation.h`

Testes:

- `scratch/test/Green/CPM/test_cpm_neutral_model_contract.cc`
- `scratch/test/Green/CPM/test_cpm_neutral_model_contract.py`
- `scratch/test/Green/CPM/test_cpm_r2_perceived_object_probe.cc`

## 3. Contratos verificados

### 3.1 Independência da API ASN.1

O contrato neutro pode ser testado sem expor os tipos ASN.1 gerados nos headers de domínio e validação. A auditoria de independência reportou:

`NEUTRAL_MODEL_ASN1_INDEPENDENCE=PASS`

Este resultado aplica-se aos ficheiros abrangidos pela auditoria. Não prova a inexistência de dependências ASN.1 no restante projeto.

### 3.2 Metadados de gestão

O modelo e os validadores cobrem:

- total de segmentos e ordinal dentro do intervalo definido;
- ordinal do segmento não superior ao total de segmentos;
- mantissa da taxa de mensagens entre 1 e 100;
- expoente da taxa entre -5 e 2;
- coerência entre as taxas mínima e máxima, comparadas à escala comum utilizada pelo validador.

### 3.3 Razões de confiança

As razões de confiança neutras são representadas separadamente dos valores físicos medidos e da respetiva confiança absoluta.

Os validadores verificam valores finitos no intervalo fechado `[0,1]` para classificações de objetos, informação de sensores e regiões de perceção. Foram testados os extremos válidos `0.0` e `1.0`, valores fora do intervalo e valores não finitos.

A representação neutra `[0,1]` não define, por si só, uma conversão normativa para campos ASN.1 com outros domínios, escalas ou significados. Essa conversão requer regras explícitas nas fases seguintes.

### 3.4 Semântica do modelo

O modelo distingue dados de entrada, conteúdo semântico da mensagem e instante de receção.

`knownObjectCount` representa o total de objetos percebidos na LDM, não simplesmente a cardinalidade dos objetos incluídos no CPM. A coerência entre o total declarado e os objetos transmitidos continua sujeita às regras de geração e inclusão aplicáveis ao cenário.

Os campos opcionais do modelo neutro, incluindo elementos da posição de referência, não garantem que uma instância incompleta possa ser codificada diretamente em ASN.1. A validação e o mapeamento para os campos obrigatórios do perfil R2 permanecem necessários.

### 3.5 Restrições de classificação CPM R2

A revisão foi confrontada com a definição ASN.1 efetivamente instalada no workspace:

`$HOME/vanetza-pki-lab/asn1/release2/TS102894-2v241-CDD.asn`

Foram corrigidas duas discrepâncias entre o modelo C++ e a alternativa `ObjectClass` do perfil R2:

- `CpmVehicleSubClassCode` contém agora apenas os valores permitidos por `TrafficParticipantType (unknown|passengerCar..tram|agricultural)`: `Unknown = 0`, `PassengerCar..Tram = 5..11` e `Agricultural = 14`. As categorias VRU continuam representadas pelas alternativas próprias do modelo.
- `CpmGroupObjectClass` já não expõe `clusterBoundingBoxShape`, porque `ObjectClass.groupSubClass` aplica a restrição `WITH COMPONENTS { ..., clusterBoundingBoxShape ABSENT }`.

O contrato C++ inclui asserções estáticas para os valores numéricos das subclasses e para a ausência de `clusterBoundingBoxShape`. Assim, alterações incompatíveis nestes pontos fazem o teste falhar durante a compilação.

## 4. Evidência executada

Resultados reexecutados em 2026-10-21, após as correções normativas e as asserções estáticas:

| Verificação | Resultado |
|---|---|
| `python3 scratch/test/run_tests.py --suite green` | 20 testes, 0 falhas, 0 erros |
| `python3 scratch/test/run_tests.py --suite all` | 42 testes, 0 falhas, 0 erros |
| Auditoria de formato | `FORMAT_AUDIT=PASS`, 10 ficheiros |
| Independência ASN.1 do modelo neutro | `NEUTRAL_MODEL_ASN1_INDEPENDENCE=PASS` |
| Testes do contrato do modelo neutro | PASS |
| Valores de `vehicleSubClass` | PASS — asserções estáticas dos valores R2 permitidos |
| Restrição de `groupSubClass` | PASS — asserção estática confirma a ausência de `clusterBoundingBoxShape` |
| Probe de objeto percebido R2, incluindo fronteiras de `objectAge` | PASS |

A execução da suite completa constitui evidência de regressão para os testes descobertos pelo runner no momento da execução. Não equivale a validação de interoperabilidade.

## 5. Limites da aceitação

Esta fase não valida:

- fonte de perceção ou integração com SUMO/TraCI;
- transformação de coordenadas SUMO para o referencial CPM;
- política de referência espacial e temporal;
- regras completas de seleção, geração e inclusão de objetos;
- mapeamento integral do modelo neutro para a estrutura ASN.1 R2 e codificação UPER;
- transporte de produção por BTP, GeoNetworking e camada de acesso;
- integração completa com `PacketContext`, security, replay, entrega ou analyzer;
- interoperabilidade externa ou conformidade ETSI global.

Os testes executados são provas de contrato e regressão isoladas. Não substituem evidência de build e runtime do caminho de produção quando esse caminho for implementado.

## 6. Critérios de aceitação

- [x] Tipos de domínio, entrada e mensagem neutros definidos.
- [x] Validadores de metadados de gestão definidos e testados.
- [x] Validadores de razões de confiança definidos e testados.
- [x] Independência ASN.1 verificada nos ficheiros abrangidos.
- [x] Valores numéricos de `vehicleSubClass` alinhados com as restrições R2 e protegidos por asserções estáticas.
- [x] `clusterBoundingBoxShape` excluído de `CpmGroupObjectClass` e a ausência protegida por asserção estática.
- [x] Testes do contrato do modelo aprovados.
- [x] Suite de regressão completa reportada sem falhas.
- [x] Limitações e dependências para as fases seguintes documentadas.

## 7. Decisão

**Decisão: PASS_WITH_SCOPE_LIMITS.**

Aceita-se a Fase 10.3 ao nível do modelo neutro e dos contratos de validação isolados, com base na evidência listada neste documento.

Esta decisão não aceita as fases posteriores, não demonstra integração do CPM no pipeline de produção e não altera o estado global do plano da Etapa 10.

**Próxima fase prevista:** 10.4 — Fonte de perceção, coordenadas e tempo.
