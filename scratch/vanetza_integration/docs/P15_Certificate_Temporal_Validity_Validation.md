---
title: "P15 — Validação da Validade Temporal dos Certificados V3"
status: "VALIDATED"
type: "VALIDATION REPORT"
date: "2026-09-30"
stage: "P15"
domain: "DENM / PKI / Vanetza Security V3"
tags:
  - ns3
  - vanetza
  - denm
  - pki
  - security
  - certificates
  - temporal-validity
  - validation
related:
  - "[[P4_Certificat_temporal_validity]]"
  - "[[P0_REBASELINE_2026-09-25]]"
  - "[[CURRENT_STATE]]"
---

# P15 — Validação da Validade Temporal dos Certificados V3

## Estado

**STATUS:** `VALIDATED`

O P15 valida o comportamento temporal do `DefaultCertificateValidator` da camada Vanetza Security V3. A integração do tempo ITS com o fluxo DENM foi introduzida anteriormente no [[P4_Certificat_temporal_validity]]. O objetivo do P15 é fechar a validação dessa política com evidência executável independente.

## Objetivo

Demonstrar que:

1. `valid_for_signing()` aplica a validação temporal;
2. um certificado expirado produz `CertificateValidator::Verdict::Expired`;
3. o comportamento possui cobertura unitária;
4. o teste é executável via CTest;
5. certificados correntes da PKI permanecem válidos;
6. um certificado expirado é efetivamente rejeitado.

## Relação com P4

O P4 estabeleceu o domínio temporal da simulação:

```text
--itsStartTime
      ↓
SimulationContext
      ↓
Vanetza ManualRuntime
      ↓
ITS absolute start + Simulator::Now()
      ↓
DefaultCertificateValidator
```

O P15 não altera esta arquitetura; valida o comportamento temporal do validator.

## Evidência da implementação

`valid_at_timepoint()` aplica semântica de intervalo:

```text
[start, end)
```

Portanto:

```text
time < start          → invalid
time = start          → valid
start < time < end    → valid
time = end            → invalid
time > end            → invalid
```

`DefaultCertificateValidator::valid_for_signing()` utiliza esta verificação temporal antes das verificações posteriores de confiança e revogação.

## Build dos testes

Árvore utilizada:

```text
/home/mmendes/vanetza-pki-lab
```

Configuração:

```bash
cmake -S . -B build-pki-lab   -DBUILD_TESTS=ON
```

Resultado:

```text
BUILD_TESTS:BOOL=ON
```

Build:

```bash
cmake --build build-pki-lab   --target GTest_DefaultCertificateValidatorV3   -j"$(nproc)"
```

Resultado:

```text
[100%] Built target GTest_DefaultCertificateValidatorV3
```

Executável:

```text
build-pki-lab/tests/GTest_DefaultCertificateValidatorV3
```

## Teste temporal

O teste existente é:

```text
DefaultCertificateValidatorTest.crl_expiry_reported_before_revocation
```

A fonte contém:

```cpp
runtime.trigger(Clock::at("2099-01-01 00:00"));

EXPECT_EQ(
    CertificateValidator::Verdict::Expired,
    cert_validator.valid_for_signing(cert, vanetza::aid::CA));
```

O teste demonstra também que a expiração é determinada antes da revogação.

Execução observada:

```text
[ RUN      ] DefaultCertificateValidatorTest.crl_expiry_reported_before_revocation
[       OK ] DefaultCertificateValidatorTest.crl_expiry_reported_before_revocation
```

A tentativa anterior com:

```bash
--gtest_filter='*Expired*'
```

retornou zero testes porque `Expired` não faz parte do nome do teste. Isso não representa uma falha.

## Suíte unitária

Execução:

```bash
./build-pki-lab/tests/GTest_DefaultCertificateValidatorV3
```

Resultado:

```text
[==========] 21 tests from 1 test suite ran.
[  PASSED  ] 21 tests.
```

Portanto:

```text
21/21 PASS
0 FAIL
```

## CTest

Execução:

```bash
ctest --test-dir build-pki-lab   -R '^DefaultCertificateValidatorV3$'   --output-on-failure
```

Resultado:

```text
Start 107: DefaultCertificateValidatorV3
1/1 Test #107: DefaultCertificateValidatorV3 ....   Passed
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.04 sec
```

## Evidência funcional da PKI

Ferramenta:

```text
/home/mmendes/vanetza-pki-lab/build-pki-lab/bin/certify-pqc
```

Para ITS-AID DENM (`37`), os tickets correntes de `veh0` foram aceites:

```text
veh0-ticket-000.cert → Valid
veh0-ticket-001.cert → Valid
veh0-ticket-002.cert → Valid
```

O fixture expirado:

```text
denm-ticket.cert → verdict 2
```

No enum do PKI Lab:

```text
0 Unknown
1 Valid
2 Expired
3 Revoked
4 Untrusted
5 InconsistentChain
6 OutsideRegion
7 InsufficientPermission
8 Misconfiguration
```

Logo:

```text
verdict 2 = CertificateValidator::Verdict::Expired
```

## Alterações de código

Nenhuma alteração de código de produção foi necessária para o P15.

Não foram alterados:

```text
certificate.cpp
certificate_validator.cpp
```

O P15 é um **validation closeout**, não uma alteração funcional de produção.

## Ressalva técnica — `get_start_and_end_validity()`

Existe uma diferença potencial entre:

```cpp
get_start_and_end_validity()
```

e:

```cpp
valid_at_timepoint()
```

para determinadas unidades de duração, especialmente:

```text
microseconds
milliseconds
years
```

No tratamento de `years`, `valid_at_timepoint()` usa aproximadamente:

```text
31556952 segundos por ano
```

enquanto `get_start_and_end_validity()` utiliza uma aproximação baseada em:

```text
365 dias
```

Também existe uma diferença de representação associada à conversão para `Time32`.

Esta discrepância não demonstrou impacto no cenário do P15, porque os certificados utilizados neste fluxo são gerados com validade baseada em horas/dias e a decisão temporal efetiva utiliza `valid_at_timepoint()`.

A questão deve permanecer registada como investigação independente e não altera o resultado do P15.

## Conclusão

O P15 encontra-se:

```text
VALIDATED
```

Evidência consolidada:

```text
Implementation         PASS
Temporal unit test     PASS
Full V3 test suite     21/21 PASS
CTest                  PASS
Current PKI tickets    VALID
Expired PKI ticket     EXPIRED
Production changes     NONE
```

O P4 permanece como o marco de implementação da integração temporal no fluxo DENM/ns-3, enquanto o P15 constitui o fecho formal da validação da semântica temporal do `DefaultCertificateValidator`.

## Estado final

```text
P4  — ITS Time Mapping + Temporal Validity
     CLOSED
     Commit: 484565a

P15 — V3 Certificate Temporal Validity Validation
     VALIDATED
     Production change: NO
     Documentation/evidence closeout: YES
```
