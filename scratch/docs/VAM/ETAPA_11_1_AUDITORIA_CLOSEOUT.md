---
title: "Etapa 11.1 — Auditoria VAM: Closeout"
project: "V2X ns-3 / Vanetza / SUMO"
stage: "11.1"
status: "AUDIT_COMPLETED / EXIT_BLOCKED"
date: 2026-11-04
baseline_commit: "7e8a13a0650323804accb18c0656d799e6fd5223"
branch: "feature/vam-etapa11-1-api-audit"
tags:
  - v2x
  - vam
  - vanetza
  - etsi
  - security
  - audit
  - closeout
---

# Etapa 11.1 — Auditoria VAM: Closeout

## 1. Estado

**AUDIT_COMPLETED / EXIT_BLOCKED**

A auditoria local da API Vanetza, ASN.1, CDD, codec UPER, BTP, GeoNetworking e integração de segurança foi executada.

O suporte ASN.1 VAM está disponível na instalação local e foi demonstrado um round-trip UPER. A infraestrutura partilhada de BTP e GeoNetworking também disponibiliza os mecanismos necessários para encaminhamento SHB e dispatch por porta.

Contudo, a saída da Etapa 11.1 permanece bloqueada. A integração de segurança atual não encaminha o ITS-AID 638 para um verificador VAM dedicado, não existe handler RX VAM na porta 2018 e os bytes de permissões SSP são descartados na fronteira neutra BTP.

Não está autorizada, por este closeout, qualquer conclusão de que VAM TX/RX seguro esteja implementado ou de que exista autorização VAM validada em runtime.

## 2. Baseline e integridade do checkout

### 2.1 Baseline observado

| Campo | Valor |
|---|---|
| Branch | `feature/vam-etapa11-1-api-audit` |
| HEAD | `7e8a13a0650323804accb18c0656d799e6fd5223` |
| Referência remota | `origin/main` |
| Baseline | Merge PR #20 — closeout CPM |
| Data da auditoria | 2026-11-04 |

### 2.2 Estado local preservado

No preflight documental foram observados:

- Modificação local em `scratch/vanetza_integration/context/simulation_controller/simulation_controller.cc`.
- Teste não rastreado `scratch/test/Green/SPATEM/RSU/test_spatem_tx_suppression.py`.
- Artefactos e scripts locais não rastreados sob `sumo/`.

Estes elementos não fazem parte da auditoria VAM e devem permanecer preservados.

**Não foram executadas operações de limpeza, reset, stash, checkout ou staging global.**

### 2.3 Documentação

Documento de baseline anterior:

` scratch/docs/ETAPA_11_0_VAM_BASELINE_FREEZE.md `

Plano de referência:

` ETAPA_11_VAM_PLANO.md `

O `.gitignore` contém uma regra global `*`. A inclusão deste closeout no Git exige staging explícito e forçado do ficheiro, sem adicionar outros artefactos.

## 3. Âmbito auditado

A auditoria cobriu:

1. Disponibilidade dos headers ASN.1 VAM e CDD.
2. Wrapper C++ Vanetza para VAM.
3. Bibliotecas e símbolos efetivamente disponíveis.
4. Implementação UPER do wrapper.
5. Round-trip VAM UPER local.
6. BTP Type B e porta de destino 2018.
7. Infraestrutura partilhada GeoNetworking SHB.
8. Contratos neutros BTP RX e GeoNetworking RX.
9. Encaminhamento de ITS-AID nos serviços de verificação e assinatura.
10. Preservação dos metadados SSP.
11. Registo de resultado de verificação em `PacketContext`.
12. Existência de handler e pipeline RX VAM.

A auditoria não implementou VAM TX/RX, não alterou os serviços de segurança e não demonstrou interoperabilidade OTA.

## 4. ASN.1, CDD e codec

### 4.1 Headers e wrapper

Foram encontrados headers VAM na instalação local:

`/usr/local/include/vanetza/asn1/its/r2/`

Entre os elementos identificados:

- `VAM.h`
- `ItsPduHeaderVam.h`
- `VamParameters.h`
- `VruAwareness.h`
- `VruHighFrequencyContainer.h`
- `VruLowFrequencyContainer.h`
- `VruClusterInformationContainer.h`
- `VruClusterOperationContainer.h`
- `VruMotionPredictionContainer.h`
- estruturas de perfil e subperfil VRU.

O wrapper C++ está disponível em:

`/usr/local/include/vanetza/asn1/vam.hpp`

A classe `vanetza::asn1::r2::Vam` deriva de `asn1c_per_wrapper<Vanetza_ITS2_VAM_t>`.

### 4.2 CDD

Os headers CDD auditados identificam o módulo `ETSI-ITS-CDD`, versão 2.4.1.

A presença dos headers gerados e das respetivas estruturas não demonstra, isoladamente, que a edição do módulo VAM corresponda a uma determinada edição normativa da TS 103 300-3.

Na auditoria original, a associação normativa ainda não estava confirmada. Foi fechada em 2026-11-09 através da identificação do módulo normativo no Annex A da TS 103 300-3 V2.3.1 e da compilação isolada com CDD V2.1.1. A evidência detalhada está no aditamento de fecho 11.1-B1.

### 4.3 Origem e linkage

O prefixo Vanetza utilizado pelo build do projeto é:

`$HOME/opt/vanetza-cpm-gcc11`

Foram identificadas bibliotecas estáticas relevantes, incluindo:

- `libvanetza_asn1_its_r2.a`
- `libvanetza_asn1_security_r2.a`
- `libvanetza_btp.a`
- `libvanetza_asn1.a`

Os hashes dos headers comparados entre `/usr/local` e o prefixo Vanetza do projeto coincidiram para os ficheiros VAM, wrapper e BTP inspecionados.

O executável de produção apresenta símbolos ASN.1 Vanetza compatíveis com linkage estático. A ausência de uma dependência Vanetza em `ldd` não deve ser interpretada como ausência de linkage.

### 4.4 UPER

A implementação local de `encode_per` e `decode_per` foi inspecionada. O objeto extraído da biblioteca efetivamente instalada contém referências a:

- `uper_encode`
- `uper_decode_complete`

Não foram identificadas referências APER nesse objeto.

### 4.5 Probe de round-trip

Foi executado um probe independente usando o wrapper VAM instalado.

Resultado observado:

```text
VAM_ORIGINAL_VALIDATE=PASS
VAM_UPER_ENCODE=PASS
VAM_UPER_SIZE_BYTES=35
VAM_UPER_DECODE=PASS
VAM_DECODED_VALIDATE=PASS
VAM_ROUNDTRIP_COMPARE=PASS
VAM_PEDESTRIAN_PROFILE_ROUNDTRIP=PASS
VAM_UPER_ROUNDTRIP=PASS
VAM_PROBE_RUNTIME_EXIT_CODE=0
VAM_PROBE_RESULT=PASS
```

O fixture incluiu o perfil pedestrian no Low Frequency Container.

Os 35 bytes representam apenas o fixture deste probe; não constituem um limite ou tamanho universal de mensagens VAM.

**Conclusão:** disponibilidade local do wrapper e round-trip UPER demonstrados. Não está demonstrada interoperabilidade independente, segurança, autorização SSP ou integração VBS.

## 5. BTP e GeoNetworking

### 5.1 BTP

A API local disponibiliza a porta VAM:

- Destination port: `2018`
- Header: BTP Type B
- Comprimento do header Type B: 4 bytes

O contrato `DataRequestB` suporta destination port, destination port info e parâmetros GeoNetworking.

O `BtpTxRequest` existente no projeto representa os parâmetros necessários para reutilizar o caminho de transporte comum.

### 5.2 GeoNetworking

O adaptador existente suporta SHB e encaminha pedidos através do Router Vanetza.

A infraestrutura existente permite representar:

- ITS-AID;
- perfil ITS-G5;
- traffic class;
- maximum packet lifetime;
- tipo de transporte;
- payload BTP.

A política VAM prevista no plano deve ser configurada explicitamente. A existência de suporte SHB no adaptador comum não demonstra, por si só, que um pacote VAM seja transmitido com todos os parâmetros normativos corretos.

### 5.3 Estado RX

Foram encontrados handlers registados para:

- CAM: 2001
- DENM: 2002
- SPATEM: 2004
- CPM: 2009

Não foi encontrado handler VAM para a porta 2018.

**Conclusão:** infraestrutura comum disponível; dispatch RX VAM ainda não implementado.

## 6. Segurança — ITS-AID 638

### 6.1 Verificação atual

O serviço `V2xV3VerifyService` tem encaminhamento explícito para CPM, ITS-AID 639.

Os restantes AIDs são encaminhados para `DenmV3VerifyService`.

No `DenmV3VerifyService`, apenas o AID DENM 37 utiliza o verificador real DENM. Os restantes AIDs recorrem ao `fallbackVerifier`, atualmente configurado com:

`DummyVerifyService(VerificationReport::Success, CertificateValidity::valid())`

Consequentemente, o AID VAM 638 não chega a um verificador VAM dedicado. O percurso atual permite que caia no fallback dummy.

Este comportamento não pode ser aceite como verificação criptográfica VAM.

### 6.2 Assinatura atual

O `DenmV3SignService` encaminha o AID DENM para o signer DENM.

Os restantes AIDs são encaminhados para `m_camFallback`, que é um `DummySignService`.

O `CpmV3SignService` tem tratamento explícito do AID 639 e rejeita outros AIDs.

Não foi encontrado signer VAM dedicado.

### 6.3 Consequência

A integração atual não demonstra:

- assinatura VAM com ITS-AID 638;
- validação de certificado e cadeia para VAM;
- validação de permissões VAM-SSP;
- rejeição criptográfica de mensagens VAM inválidas;
- política VAM de pseudónimo e certificado.

**Decisão:** o VAM não pode utilizar o fallback dummy para produzir um resultado de segurança considerado válido.

O tratamento de AID 638 deverá ser explícito e fail-closed até existir um verificador VAM devidamente configurado.

## 7. SSP — perda de informação na fronteira neutra

### 7.1 API de origem

A indicação GeoNetworking disponibiliza permissões como:

`optional<ByteBuffer>`

A API de verificação disponibiliza também `VerifyConfirm.permissions`, mas este campo representa o resultado produzido pelo serviço de segurança e não deve ser confundido automaticamente com os bytes recebidos na indicação GeoNetworking.

### 7.2 Contratos neutros atuais

`VanetzaBtpAdapter::RxData` contém:

```cpp
bool hasPermissions = false;
std::size_t permissionsSize = 0;
```

`VanetzaGeoNetAdapter::BtpRxData` contém os mesmos campos.

`GeoNetRxContext` contém:

```cpp
bool hasPermissions = false;
std::size_t permissionsSize = 0;
```

`PacketContext.geonet` utiliza `GeoNetRxContext`.

### 7.3 Ponto de perda

Na conversão de `DataIndication` para `VanetzaBtpAdapter::RxData`, o código:

1. verifica se `ind.permissions` existe;
2. define `hasPermissions`;
3. copia apenas `ind.permissions->size()` para `permissionsSize`.

Os bytes SSP não são copiados.

As camadas seguintes propagam apenas os campos reduzidos.

### 7.4 Decisão de arquitetura

Uma futura alteração deverá distinguir explicitamente:

1. permissões/SSP recebidas na indicação GeoNetworking;
2. permissões devolvidas pela verificação criptográfica;
3. resultado da política de autorização VAM aplicada à mensagem.

Não tratar a mera presença de bytes SSP como prova de autorização.

Não reduzir a decisão de segurança a `hasPermissions=true`.

O contrato neutro deverá preservar a proveniência dos dados e permitir uma decisão de autorização auditável, sem expor tipos Vanetza para fora dos wrappers.

## 8. PacketContext e RX

O `PacketContext` já contém:

- `GeoNetRxContext geonet`;
- `verificationPerformed`;
- `verificationSucceeded`;
- `verificationReport`;
- `verifiedItsAid`.

Os serviços CPM e DENM registam resultados de verificação no contexto obtido através de `RxBridgeContext::Current()`.

O pipeline CPM exige resultado de verificação presente, bem-sucedido e com AID esperado antes de avançar para decode e entrega.

Esta abordagem constitui uma referência arquitetural útil para o futuro VAM, mas não deve ser copiada cegamente: o encaminhamento de AID 638 e a política SSP necessitam de tratamento próprio.

Não existe ainda pipeline RX VAM que demonstre a sequência completa:

`RAW_RX → ACCESS_RX → GEONET_RX → BTP_RX → VERIFY → DECODE → REPLAY → RECEPTION → DELIVER`

## 9. Matriz de auditoria

| Elemento | Evidência | Estado |
|---|---|---|
| Headers ASN.1 VAM | Headers R2 encontrados | PASS |
| Wrapper C++ VAM | `vanetza/asn1/vam.hpp` | PASS |
| CDD | Prefixo instalado: V2.4.1/OID 4.3; par normativo isolado: V2.1.1/OID 3.1 | PASS isolado; prefixo instalado incompatível |
| Codec UPER | Wrapper UPER instalado; runtime do par normativo isolado ainda por exercitar | PASS local / OPEN normativo |
| Round-trip VAM | Probe pedestrian na instalação local; não exercita o par normativo isolado | PASS local |
| Edição normativa VAM/ASN.1 | TS 103 300-3 V2.3.1 + módulo Annex A SHA 5631c6d2... + CDD V2.1.1/OID 3.1/SHA 13d68934... | RESOLVED (11.1-B1) |
| Compilação ASN.1 normativa | 469 fontes; GCC 11; manifesto sem diferenças; descriptors VAM exportados | PASS isolado |
| Prefixo Vanetza de produção | CDD V2.4.1/OID 4.3 não corresponde ao import normativo OID 3.1 | INCOMPATÍVEL |
| BTP Type B | API e header disponíveis | PASS |
| BTP port 2018 | Header local confirma porta | PASS |
| GeoNetworking SHB | Adaptador comum disponível | PASS infraestrutura |
| VAM TX | Sem signer VAM dedicado | BLOCKED |
| VAM RX port 2018 | Handler inexistente | BLOCKED |
| AID 638 verification | Percurso atual usa fallback dummy | BLOCKED |
| AID 638 signing | Sem signer VAM dedicado | BLOCKED |
| SSP recebido | Bytes descartados no boundary BTP | BLOCKED |
| SSP verificado | Sem política VAM demonstrada | BLOCKED |
| Autorização VAM | Não demonstrada | BLOCKED |
| Replay VAM | Pipeline VAM inexistente | BLOCKED |
| Interoperabilidade independente | Não testada | NOT DEMONSTRATED |

## 10. Decisões e trabalho pendente

### 10.1 Decisões já estabelecidas

1. Reutilizar `VanetzaBtpAdapter` e `VanetzaGeoNetAdapter`.
2. Reutilizar `PacketContext` e `RxBridgeContext`.
3. Não criar `VamBtpAdapter`, `VamSecurityRuntime` ou `VamPacketContext` paralelos.
4. Não aceitar fallback dummy como verificação VAM.
5. Não considerar bytes SSP recebidos como autorização validada.
6. Preservar a separação entre stationId, GN_ADDR.MID, Link MAC, packetId e pseudónimo de certificado.
7. Não iniciar o E2E VAM antes de existir assinatura e verificação reais.

### 10.2 Bloqueios a resolver

| ID | Bloqueio | Resultado necessário | Estado |
|---|---|---|---|
| 11.1-B1 | Edição normativa VAM/ASN.1 | TS 103 300-3 V2.3.1, módulo Annex A e CDD V2.1.1 identificados e compilados isoladamente | RESOLVED |
| 11.1-B2 | Encaminhamento AID 638 | Política explícita, sem fallback dummy | BLOCKED |
| 11.1-B3 | Signer VAM | Assinatura real com AID 638 e SSP correto | BLOCKED |
| 11.1-B4 | Verifier VAM | Verificação real de certificado e assinatura | BLOCKED |
| 11.1-B5 | SSP | Contrato neutro com proveniência preservada | BLOCKED |
| 11.1-B6 | Autorização | Decisão VAM-SSP demonstrável | BLOCKED |
| 11.1-B7 | RX VAM | Handler port 2018 integrado no pipeline comum | BLOCKED |
| 11.1-B8 | Regressões | Plano de regressão CAM/DENM/SPATEM/CPM aprovado | OPEN |

## 11. Sequência de continuação

A continuação deverá respeitar esta ordem:

1. **11.1-B — decisão normativa:** `COMPLETED` em 2026-11-09. O bloqueio 11.1-B1 está `RESOLVED` para o par publicado VAM V2.3.1 + CDD V2.1.1, com compilação ASN.1 isolada demonstrada.
2. **11.1-C — contrato de segurança:** próximo passo; definir encaminhamento AID 638, signer, verifier e semântica SSP.
3. **11.1-D — regressões:** especificar testes negativos e positivos para os serviços partilhados.
4. **11.1-E — gate de saída:** rever evidências e decidir formalmente se a Etapa 11.1 pode ser encerrada.
5. **11.2 — perfil e papel VRU:** só iniciar após aceitação do gate anterior.

As alterações aos serviços de segurança partilhados não deverão ser misturadas com o registo documental desta auditoria.

## 12. Critério de saída da Etapa 11.1

A Etapa 11.1 só poderá passar a `CLOSED / VALIDATED` quando:

- a edição VAM e o CDD compatível estiverem identificados — SATISFEITO (11.1-B1);
- o codec UPER do par normativo estiver ligado e exercitado com round-trip — PENDENTE;
- o perfil BTP/GeoNetworking estiver definido;
- o AID 638 tiver encaminhamento de segurança explícito;
- o signer e verifier reais estiverem identificados e ligados;
- a política SSP estiver definida e os dados necessários forem preservados;
- o percurso não utilizar fallback dummy para VAM;
- existir um probe de compilação/linkagem coerente com a configuração selecionada — compilação C ASN.1 isolada PASS; linkage e round-trip do codec normativo ainda pendentes;
- o plano de regressão dos serviços existentes estiver aprovado.

O round-trip UPER isolado não satisfaz estes critérios.

## 13. Limites da evidência

Esta auditoria demonstra disponibilidade de estruturas ASN.1, codec UPER local, APIs BTP e infraestrutura comum de GeoNetworking.

Não demonstra:

- conformidade ETSI;
- interoperabilidade OTA;
- assinatura VAM válida;
- autorização VAM-SSP;
- receção e entrega VAM em runtime;
- proteção VAM contra replay;
- integração com VBS;
- integração com SUMO.

Qualquer afirmação futura de `PASS` deverá incluir comando, resultado, artefacto e critério efetivamente verificado.

## 14. Integridade e commits

Este documento é um registo documental da auditoria.

Não deve incluir alterações aos serviços de segurança, aos adapters partilhados, ao pipeline RX ou ao target de produção.

A inclusão no Git deverá ser limitada a este ficheiro, usando staging explícito e forçado devido à regra global do `.gitignore`.

Mensagem de commit proposta:

`docs(vam): record etapa 11.1 audit and exit blockers`

O commit só deverá ser criado após revisão do conteúdo e autorização explícita.

---

## Aditamento — Validação semântica do header VAM

**Data:** 2026-11-09

**Branch de validação:** `feature/vam-etapa11-1b-header-validation`

**Baseline desta validação:** `6da8aa1a5f3152183053a736c68982bfd6096908`

Este aditamento complementa a auditoria original. Não altera a baseline histórica, não modifica a seleção normativa e não altera o estado `AUDIT_COMPLETED / EXIT_BLOCKED`.

### A. Descriptor ASN.1 instalado

A inspeção do prefixo selecionado pelo CMake (`$HOME/opt/vanetza-cpm-gcc11`) confirmou:

- `Vanetza_ITS2_ItsPduHeaderVam_t` é um alias de `Vanetza_ITS2_ItsPduHeader_t`.
- O header contém `protocolVersion`, `messageId` e `stationId`.
- `Vanetza_ITS2_MessageId_vam` corresponde ao valor 16.
- O descriptor `asn_DEF_Vanetza_ITS2_ItsPduHeaderVam` está exportado por `libvanetza_asn1_its_r2.a`.
- Os quatro headers comparados entre o prefixo selecionado e `/usr/local` apresentam hashes coincidentes.

### B. Limitação de WITH COMPONENTS

Foi compilado e ligado um probe independente contra as bibliotecas instaladas no prefixo selecionado pelo CMake.

Resultado de `asn_check_constraints()`:

| protocolVersion | messageId | Resultado observado |
|---:|---:|---|
| 3 | 16 | ACCEPT |
| 2 | 16 | ACCEPT |
| 3 | 2 | ACCEPT |
| 2 | 2 | ACCEPT |

O descriptor instalado não aplica a restrição VAM `WITH COMPONENTS` que exige `protocolVersion = 3` e `messageId = vam(16)`.

Consequentemente, `asn_check_constraints()` não é suficiente para validar estes dois requisitos semânticos. Não foi alterado o código ASN.1 gerado nem o compilador.

### C. Validador semântico isolado

Foi criada a unidade:

- `scratch/vanetza_integration/vam/codec/vam_header_validator.h`
- `scratch/vanetza_integration/vam/codec/vam_header_validator.cc`

A API recebe diretamente `Vanetza_ITS2_ItsPduHeaderVam_t`, sem introduzir uma estrutura paralela para o header.

As regras implementadas são:

- `protocolVersion == 3`;
- `messageId == Vanetza_ITS2_MessageId_vam`.

A validação da versão precede a validação do message ID. `stationId` permanece fora do âmbito desta unidade.

### D. Evidência de testes

Harnesses:

- `scratch/test/Red/VAM/test_vam_header_validator.cc`
- `scratch/test/Red/VAM/test_vam_header_validator.py`

Resultados observados:

~~~~text
[PASS] VALID_VAM_HEADER success=1 error=""
[PASS] WRONG_PROTOCOL_VERSION success=0 error="unsupported VAM protocolVersion"
[PASS] WRONG_MESSAGE_ID success=0 error="invalid VAM messageID"
[PASS] BOTH_FIELDS_INVALID success=0 error="unsupported VAM protocolVersion"
VAM_HEADER_VALIDATION_CASES=4
VAM_HEADER_VALIDATION_PASSED=4
VAM_HEADER_VALIDATION_FAILED=0
VAM_HEADER_VALIDATION_TESTS=PASS
~~~~

Verificação adicional:

| Gate | Resultado |
|---|---|
| Python syntax | PASS |
| GCC 11 / C++23 | PASS |
| `-Wall -Wextra -Werror` | PASS |
| AddressSanitizer | PASS |
| UndefinedBehaviorSanitizer | PASS |
| Exit code de compilação | 0 |
| Exit code de execução | 0 |

Os headers Vanetza foram tratados como system includes (`-isystem`) para não transformar warnings internos da dependência em erros do código do projeto.

### E. Limites e bloqueios preservados

Esta evidência demonstra apenas o contrato semântico isolado do header VAM.

Não demonstra:

- integração do validador num encoder ou decoder VAM de produção;
- round-trip UPER através de um codec pertencente ao projeto nesta branch;
- integração TX/RX, BTP ou GeoNetworking VAM;
- assinatura ou verificação criptográfica VAM;
- autorização ITS-AID 638 / SSP;
- interoperabilidade independente.

A correspondência normativa entre a edição publicada VAM, o módulo ASN.1 e o CDD instalado permanece aberta. O bloqueio 11.1-B1 não é considerado resolvido por esta validação.

Os restantes bloqueios de segurança, dispatch RX e SSP também permanecem inalterados.

### F. Decisão

**Header semantic validator: ISOLATED UNIT VALIDATED.**

**Etapa 11.1: EXIT BLOCKED.**

A integração no codec VAM deverá reutilizar este validador na fronteira ASN.1, após resolução da combinação normativa. Não se justifica criar neste momento um serviço VAM, um pipeline específico ou alterações ao `PacketContext` para acomodar apenas esta unidade.

---

## Aditamento — Fecho normativo do bloqueio 11.1-B1

**Data:** 2026-11-09

**Branch de validação:** `feature/vam-etapa11-1b-header-validation`

**Commit de partida:** `431ab18713eaedb50cd8bcb77b891cb53e2a6135`

### A. Seleção normativa

Foi confirmada a combinação publicada:

| Elemento | Identificação | OID | SHA-256 |
|---|---|---|---|
| ETSI TS 103 300-3 | V2.3.1, Annex A normativo | VAM `3.1` | Documento ETSI |
| Designação ETSI do módulo VAM | `ETSI-ITS-VAM-PDU-Descriptions` (identificador declarado no ficheiro ASN.1: `VAM-PDU-Descriptions`) | `3.1` | `5631c6d2534a7883fe329f9fe40f45b039e3c9d0611560ea7ef20d3a9ba3d11f` |
| ETSI TS 102 894-2 / CDD | V2.1.1 | CDD `3.1` | Documento ETSI |
| Módulo ASN.1 CDD | `ETSI-ITS-CDD` | `3.1` | `13d6893406459a7b99bc322c3bae8e63f95114bad2ffc6e258b53ab35f02c5a0` |

Fontes oficiais:

- ETSI TS 103 300-3 V2.3.1: https://www.etsi.org/deliver/etsi_ts/103300_103399/10330003/02.03.01_60/ts_10330003v020301p.pdf
- Módulo VAM indicado na Tabela A.1 do Annex A: https://forge.etsi.org/rep/ITS/asn1/vam-ts103300_3/-/tree/v2.3.1
- ETSI TS 102 894-2 V2.1.1: https://www.etsi.org/deliver/etsi_ts/102800_102899/10289402/02.01.01_60/ts_10289402v020101p.pdf
- Módulo CDD: https://forge.etsi.org/rep/ITS/asn1/cdd_ts102894_2/-/tree/v2.1.1

O Annex A, Tabela A.1 da TS 103 300-3 V2.3.1, identifica expressamente o nome do módulo VAM, o OID `3.1`, o URL de origem e o SHA-256 acima. O hash do ficheiro obtido para a compilação coincide exatamente com o hash publicado pela ETSI.

O módulo VAM contém um comentário inicial antigo que referencia um draft e uma versão anterior. Para a identificação normativa prevalecem o OID e o SHA-256 publicados na Tabela A.1 do Annex A, que coincidem com o ficheiro utilizado.

### B. Compatibilidade do import CDD

O módulo VAM oficial importa `ETSI-ITS-CDD` com OID:

`{ itu-t(0) identified-organization(4) etsi(0) itsDomain(5) wg1(1) 102894 cdd(2) major-version-3(3) minor-version-1(1) }`

A importação utiliza `WITH SUCCESSORS`. Segundo a regra de versionamento ASN.1 documentada pela ETSI, todos os arcos anteriores ao último têm de coincidir; o último arco pode ser igual ou superior ao indicado no import.

O CDD oficial V2.1.1 declara exatamente OID `3.1`, pelo que satisfaz o import sem qualquer adaptação.

O prefixo de produção atualmente selecionado pelo CMake contém CDD V2.4.1, OID `4.3`. Este OID não satisfaz o import `3.1`, porque o arco major é diferente.

O ficheiro VAM local em `~/vanetza/asn1/release2/TS103300-3v231/VAM-PDU-Descriptions.asn`, SHA-256 `7a6f83c2927a1ac37beba903acd3b3ba15c4616bb7dee98b75ee25df5dcbb0d7`, altera o import do módulo VAM de OID `3.1` para `4.3`. É uma adaptação local, não o ficheiro normativo publicado, e não foi utilizado na compilação normativa aqui registada.

### C. Compilação ASN.1 isolada

Diretório de compilação:

`/tmp/vam-11-1b-r2-final-compile.KIQxSC`

Fontes usadas:

- VAM oficial, SHA-256 `5631c6d2534a7883fe329f9fe40f45b039e3c9d0611560ea7ef20d3a9ba3d11f`;
- CDD oficial V2.1.1, SHA-256 `13d6893406459a7b99bc322c3bae8e63f95114bad2ffc6e258b53ab35f02c5a0`.

Os dois ficheiros na diretoria de schema usada pelo probe foram comparados byte a byte com as cópias oficiais; ambos coincidiram.

Proveniência do compilador:

- ASN.1 compiler image: `vanetza-asn1c:audit-eefc4cf-fix5966fef-v2`;
- image ID: `sha256:43c8a0d0adf37e6b65dcd344c57ebf295da6328eb5fcb555a77f4c96d7076b68`;
- base asn1c commit: `eefc4cfca40e5ee6e0b6506d6d8c93c24d423cec`;
- isolated fix commit: `5966fef39d05ccd41d679484eb1b3f29c3143b20`;
- ficheiro alterado no compilador isolado: `libasn1fix/asn1fix_constr.c`;
- C compiler: `/usr/bin/gcc-11`.

Resultados:

| Verificação | Resultado |
|---|---|
| Fontes VAM e CDD iguais às cópias oficiais | PASS |
| Import closure dos 34 tipos CDD | PASS |
| Fontes C geradas | 469 |
| Manifesto esperado | 469 |
| Diferença entre manifestos | 0 |
| Target `asn1_its_r2` | BUILT |
| `asn_DEF_Vanetza_ITS2_ItsPduHeaderVam` exportado | PASS |
| `asn_DEF_Vanetza_ITS2_VAM` exportado | PASS |

Hashes dos artefactos isolados:

| Biblioteca | SHA-256 |
|---|---|
| `libasn1_its_r2.a` | `5e440938958cae0cf816445f65fa237ad0e835ef5b8ef46e54f656831364336e` |
| `libasn1_support.a` | `5bde5502c6b7b08f24bdff4825aad60e4825e0713018c832e165ffbfba0905ee` |

### D. Limites da conclusão

Esta compilação demonstra que o par oficial VAM V2.3.1 + CDD V2.1.1 pode ser processado pelo compilador isolado e que os tipos e descriptors VAM são gerados e exportados pela biblioteca ASN.1 resultante.

Não demonstra ainda:

- ligação do wrapper C++ Vanetza contra estas bibliotecas normativas isoladas;
- round-trip UPER de um VAM completo com este par específico;
- substituição ou compatibilidade do prefixo Vanetza de produção;
- assinatura, verificação, SSP ou autorização ITS-AID 638;
- TX/RX VAM integrado ou interoperabilidade independente.

O prefixo de produção não foi alterado. Nenhuma biblioteca foi instalada ou substituída.

### E. Decisão

**Bloqueio 11.1-B1: RESOLVED.**

A edição publicada VAM, o módulo ASN.1 e a versão CDD importada estão identificados, os hashes coincidem com as fontes oficiais e a compilação ASN.1 isolada passou.

A integração desta combinação no wrapper/codec do projeto permanece um trabalho separado. O validador semântico de header do commit `431ab18` continua a ser uma unidade isolada; a sua integração no futuro codec deve usar os tipos gerados a partir deste par normativo.

**Etapa 11.1: AUDIT_COMPLETED / EXIT_BLOCKED.**

Os bloqueios 11.1-B2 a 11.1-B8 permanecem abertos ou bloqueados conforme a matriz atualizada.
