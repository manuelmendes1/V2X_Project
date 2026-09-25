---
title: "P1 — Modelo Explícito VehicleCredential"
status: "COMPLETED"
type: "ARCHITECTURAL IMPLEMENTATION"
date: 2026-09-25
stage: "P1"
domain: "DENM / PKI / Vehicle Security"
tags:
  - ns3
  - vanetza
  - denm
  - pki
  - security
  - vehicle-credential
  - architecture
  - validation
related_versions:
  - "[[P0_REBASELINE_2026-09-25]]"
related_state:
  - "[[CURRENT_STATE]]"
next_stage:
  - "[[P2_Credential_Pool_and_Discovery]]"
---


# P1 — Modelo Explícito VehicleCredential

## Objetivo

Introduzir `VehicleCredential` como tipo de domínio explícito para representar uma credencial de veículo no subsistema DENM/PKI, sem alterar ainda as políticas superiores de pool, descoberta, rotação ou recuperação de falhas.

Esta etapa sucede ao baseline estabelecido em [[P0_REBASELINE_2026-09-25]].

## Resultado

A representação de credencial deixou de depender do tipo histórico `DenmCredential`.

O novo tipo canónico é:

```text
VehicleCredential
├── credentialId
├── vehicleId
├── sequence
├── certFile
└── keyFile
```

O tipo é um descritor de domínio. Não possui `EVP_PKEY*` nem `X509*`, não executa filesystem I/O e não gere diretamente material criptográfico carregado.

A responsabilidade pelo material criptográfico continua em [[VehicleSecurityContext]].

## Ownership

```text
VehicleCredential
    │
    ├── identidade da credencial
    ├── identidade do veículo
    ├── sequência
    └── referências aos ficheiros
             │
             ▼
VehicleSecurityContext
    │
    ├── pool de VehicleCredential
    ├── índice da credencial ativa
    ├── EVP_PKEY*
    └── X509*
```

A separação é intencional:

`VehicleCredential` descreve a credencial.

`VehicleSecurityContext` mantém o estado de segurança carregado e a credencial ativa.

O signer DENM recebe uma referência constante ao descritor através de `DenmV3SignService`.

## Alterações implementadas

O header histórico `denm_credential.h` foi removido e substituído por:

```text
context/vehicle_security/vehicle_credential.h
```

As estruturas e interfaces relevantes foram migradas para `VehicleCredential`, mantendo os nomes específicos do domínio DENM, tais como:

```text
denmCredentials
activeDenmCredentialIndex
SetActiveDenmCredential()
RotateToNextDenmCredential()
RotateDenmCredential()
DiscoverDenmCredentials()
```

A migração de tipo não provocou uma renomeação global das APIs DENM.

## Identidade explícita

A descoberta de credenciais sequenciadas passa a produzir descritores como:

```text
credentialId = veh0-ticket-000
vehicleId    = veh0
sequence     = 0
```

e, para as credenciais seguintes:

```text
veh0-ticket-001 -> sequence 1
veh0-ticket-002 -> sequence 2
```

Isto torna explícita a relação entre veículo, credencial e sequência.

## Compatibilidade

O modelo mantém temporariamente um construtor compatível baseado apenas em:

```text
certFile
keyFile
```

Esta compatibilidade não altera o modelo canónico. A descoberta de credenciais e o estado persistente de segurança usam o descriptor completo.

A remoção dessa compatibilidade permanece fora do objetivo de P1.

## Fora do âmbito de P1

Não foram introduzidas nesta etapa:

* política global de credential pool;
* descoberta dinâmica de credenciais;
* provider de `ActiveCredential`;
* rotação automática por veículo;
* binding criptográfico certificado ↔ veículo;
* política de falha durante rotação;
* expiração ou validade temporal;
* integração de novas credenciais no bridge.

Esses aspetos pertencem às etapas posteriores, nomeadamente [[P2_Credential_Pool_and_Discovery]] e etapas subsequentes do plano DENM/PKI.

## TDD

A implementação seguiu:

```text
RED
↓
teste focal de VehicleCredential
↓
GREEN
↓
migração dos consumidores
↓
regressão DENM
```

O RED inicial foi demonstrado pelo erro de compilação devido à ausência de:

```text
vehicle_credential.h
```

O GREEN foi obtido após introdução do modelo e migração dos consumidores.

## Evidência de validação

### Modelo focal

```text
scratch_vehicle_credential_model_test
```

Resultado:

```text
7 assertions PASS
VEHICLE_CREDENTIAL_MODEL_EXIT=0
```

### Descoberta

```text
scratch_denm_credential_discovery_test
```

Validou:

```text
credentialId
vehicleId
sequence
certFile
keyFile
```

para `veh0-ticket-000`, `001` e `002`.

### Pool e rotação

Passaram:

```text
scratch_denm_credential_pool_rotation_test
scratch_denm_v3_credential_rotation_test
scratch_denm_runtime_credential_rotation_test
scratch_denm_v3_runtime_e2e_rotation_test
scratch_denm_v3_scheduler_e2e_rotation_test
scratch_denm_high_density_v3_runtime_rotation_test
scratch_denm_high_density_v3_runtime_sign_test
scratch_denm_multi_vehicle_v3_runtime_rotation_test
scratch_denm_multi_vehicle_v3_runtime_sign_test
```

### Configuração

```text
scratch_vehicle_security_initializer_config_test
```

Resultado:

```text
controller-provided PKI directory selects veh0 credential 000 certificate
controller-provided PKI directory selects veh0 credential 000 private key
selected credential has explicit credential identifier
selected credential is explicitly bound to veh0
selected credential carries sequence 000
VEHICLE_SECURITY_PK_CONFIG_CONTROLLER_EXIT=0
```

### Analyzer

```text
python -m pytest scratch/v2x_analyzer/tests/ -q
```

Resultado:

```text
26 passed
ANALYZER_EXIT=0
```

### Bridge

```text
test_denm_bridge_acceptance.sh
```

Resultado:

```text
TEST_COUNT=5
FAILURES=0
DENM_BRIDGE_ACCEPTANCE_EXIT=0
```

### CAM regression

```text
test_cam_regression_acceptance.sh
```

Resultado:

```text
TX = 144
RX = 4
CAM identity/transport = PASS
DENM/GBC events = 0
vehicles = veh0 veh1 veh2 veh3
RESULTADO 8.13: PASS
ACCEPTANCE_EXIT=0
```

## Integridade estrutural

A verificação:

```text
DenmCredential
context/vehicle_security/denm_credential.h
```

não encontra referências residuais.

Resultado:

```text
OLD_TYPE_FOUND=0
```

Também foi executado:

```text
git diff --check
```

Resultado:

```text
DIFF_CHECK_EXIT=0
```

## Relação com o baseline

[[P0_REBASELINE_2026-09-25]] estabelece o estado funcional anterior à implementação do modelo explícito.

P1 altera a representação de domínio da credencial, preservando:

```text
DENM generation
DENM signing
DENM verification
credential discovery
credential rotation
CAM regression
bridge acceptance
analyzer regression
```

## Próxima etapa

A próxima etapa deve tratar o pool e a descoberta como conceitos arquiteturais próprios, usando `VehicleCredential` como elemento canónico:

[[P2_Credential_Pool_and_Discovery]]

A rotação automática por veículo, binding criptográfico e políticas de recuperação permanecem posteriores.

## Estado final

```text
P1 = COMPLETED

Model:
    VehicleCredential = canonical

Legacy type:
    DenmCredential = removed

Build:
    PASS

DENM regression:
    PASS

Analyzer:
    26 passed

Bridge:
    5/5 passed

CAM:
    PASS

Static consistency:
    PASS

Diff check:
    PASS
```
