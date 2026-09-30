---
title: "P16 — Provisioning de Credenciais DENM em Alta Densidade"
status: "VALIDATED"
type: "CROSS-REPOSITORY VALIDATION REPORT"
date: "2026-09-30"
stage: "P16"
domain: "DENM / PKI / Provisioning"
tags:
  - ns3
  - vanetza
  - denm
  - pki
  - provisioning
  - credentials
  - high-density
  - validation
related:
  - "[[P15_Certificate_Temporal_Validity_Validation]]"
  - "[[P1_Modelo_Explicito_VehicleCredential]]"
  - "[[CURRENT_STATE]]"
  - "[[DOCUMENT_STATUS]]"
  - "[[P17_P4_Bridge_Integration]]"
---

# P16 — Provisioning de Credenciais DENM em Alta Densidade

## Estado

**STATUS:** VALIDATED

O P16 valida o mecanismo de provisioning multi-veículo da PKI DENM para cenários de maior densidade. A validação foi realizada no repositório externo:

~~~
/home/mmendes/vanetza-pki-lab
~~~

O código do provisioning não é copiado para V2X_Project. O ns-3 mantém a referência arquitetural e a evidência do comportamento; a infraestrutura de emissão de certificados permanece no repositório PKI.

## Objetivo

Demonstrar que o provisioning consegue:

1. criar credenciais para um conjunto arbitrário de veículos;
2. emitir três credenciais DENM por veículo;
3. criar a chave privada correspondente em PEM;
4. validar cadeia e chave durante o processo de provisioning;
5. preservar credenciais válidas em execuções posteriores;
6. regenerar credenciais inválidas;
7. impedir que veículos fora do intervalo solicitado sejam provisionados.

## Implementação validada

Ferramenta:

~~~
tools/provision-denm-multi-vehicle.sh
~~~

Parâmetros:

~~~
<pki-dir> [credentials-per-vehicle] [vehicle-count]
~~~

Para cada veículo veh0 ... vehN-1, o provisioner processa as sequências 000, 001 e 002.

Um par certificado/chave existente é validado antes de ser mantido. Um par inválido é tratado como não utilizável e é regenerado.

## Evidência de alta densidade

Foi executada uma validação a partir de um diretório temporário contendo apenas:

~~~
denm-root.cert
denm-aa.cert
aa.key
~~~

O provisioning foi invocado para:

~~~
vehicle-count = 100
credentials-per-vehicle = 3
~~~

Resultado:

~~~
veh0 ... veh99
100 veículos provisionados
300 certificados
300 chaves PEM
~~~

Foi igualmente verificado que veh100 não foi provisionado.

A execução apresentou:

~~~
P16_HIGH_DENSITY_PROVISIONING=PASS
ELAPSED=0:32.17
CPU=99%
~~~

A amostra criptográfica adicional validou cadeia e correspondência chave/certificado para veh0, veh50 e veh99, em cada uma das sequências 000, 001 e 002.

## Teste automatizado permanente

O teste:

~~~
tools/test-provision-denm-dynamic.sh
~~~

foi ampliado para validar explicitamente o cenário de alta densidade.

O fluxo cobre:

~~~
provisionamento inicial: 50 veículos × 3 credenciais
extensão:                100 veículos × 3 credenciais
idempotência:            100 veículos × 3 credenciais
limite negativo:         veh100 não provisionado
~~~

Resultado observado:

~~~
100 verificações de veículo: PASS
veh100 ausente: PASS
segunda execução idempotente: PASS
300 pares preservados: PASS
exit code: 0
~~~

A condição esperada para a segunda execução é 300 entradas KEEP, confirmando que os pares já válidos não são regenerados.

## Semântica operacional

A arquitetura atual distingue claramente:

~~~
PKI provisioning
    ↓
VehicleCredential material
    ↓
VehicleSecurityInitializer / credential discovery
    ↓
VehicleCredentialPool / active credential state
    ↓
DENM signing
~~~

O P16 valida somente a primeira fronteira: criação e manutenção do material criptográfico para uma população de veículos.

A integração desse provisioning com a criação dos veículos do cenário SUMO/ns-3 pertence ao P17.

## Relação com o P17

O P16 não valida:

~~~
p4_bridge_high_density.py
SUMO veh0 ... vehN
ns-3 runtime
DENM runtime delivery
interoperabilidade E2E
~~~

Esses pontos permanecem como escopo do [[P17_P4_Bridge_Integration]].

Em particular, o script p4_bridge_high_density.py não deve ser considerado automaticamente integrado ao provisioning apenas porque a PKI consegue gerar 100 veículos.

## Repositório de evidência

Repositório:

~~~
/home/mmendes/vanetza-pki-lab
~~~

Commit do teste de alta densidade:

~~~
08eff6ae
test(p16): validate high-density DENM provisioning
~~~

Esse commit pertence ao repositório PKI e não ao histórico de V2X_Project.

## Conclusão

O P16 encontra-se:

~~~
VALIDATED
~~~

Evidência consolidada:

~~~
3 credentials / vehicle       PASS
100 vehicles                   PASS
300 certificate/key pairs      PASS
veh100 exclusion               PASS
idempotent re-run              PASS
invalid-pair healing           IMPLEMENTED
ns-3/SUMO integration          NOT YET VALIDATED
~~~

O resultado fecha o requisito de provisioning em alta densidade no domínio PKI. O próximo limite de validação é a ligação determinística entre a população SUMO, a identidade do veículo e o material criptográfico, tratada em P17.

## Estado final

~~~
P15 — Certificate Temporal Validity
      VALIDATED

P16 — High-Density DENM Provisioning
      VALIDATED
      Evidence repository: vanetza-pki-lab
      Evidence commit: 08eff6ae

P17 — p4_bridge / SUMO integration
      NEXT
~~~
