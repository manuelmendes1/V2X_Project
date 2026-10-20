---
title: "Etapa 10.1 — Auditoria da API ASN.1 R2 do CPM"
status: "PASS_WITH_SCOPE_LIMITS"
tags: [V2X, CPM, ASN.1, Vanetza, UPER, testing]
---

# Etapa 10.1 — Auditoria da API ASN.1 R2 do CPM

## Decisão

**PASS para a disponibilidade da API R2 e para o round-trip de uma mensagem CPM mínima.** Não fica demonstrada, nesta etapa, a serialização de objetos percebidos nem a integração no pipeline ns-3/V2X.

Base de referência do repositório: commit `0f706cfa0d5589d6304cf0e92b342d4348f08373` (`fix(cam): keep scenario coordinates within ASN.1 bounds`). O checkout estava limpo e alinhado com `origin/main` antes desta alteração de documentação/teste.

## API e modelo ASN.1 confirmados

Os headers CPM R2 estão instalados em `/usr/local/include/vanetza/asn1/its/r2/`. O wrapper público `vanetza::asn1::r2::Cpm` usa `asn1c_per_wrapper<Vanetza_ITS2_CollectivePerceptionMessage_t>`.

Foram confirmados os seguintes elementos:

- `CollectivePerceptionMessage` com cabeçalho e `CpmPayload`.
- Versão de protocolo CPM R2 igual a 2 e `messageId` CPM igual a 14.
- `CpmPayload` contém `managementContainer` e `cpmContainers`.
- O tempo de referência `TimestampIts` é gerado como `INTEGER_t`; deve ser inicializado através das funções ASN.1 de conversão de inteiros, não por atribuição direta a um `long`.
- A mensagem de prova utiliza um `OriginatingRsuContainer` (container ID 2), sem `mapReference`.
- As restrições da mensagem completa foram verificadas com `asn_check_constraints` antes da codificação.

## Evidência executada

### Probe de descodificação malformada

| Verificação | Resultado |
|---|---|
| Compilação do probe | Exit code 0 |
| Execução do probe | Exit code 0 |
| Payload vazio aceite | Não |
| Payload truncado aceite | Não |
| Estado reportado | `R2_MALFORMED_DECODE_PROBE=PASS` |

### Probe de round-trip CPM R2 mínimo

| Verificação | Resultado |
|---|---|
| Compilação | Exit code 0 |
| Execução | Exit code 0 |
| Validação ASN.1 | PASS |
| Codificação UPER | PASS |
| Tamanho codificado | 30 bytes |
| Versão descodificada | 2 |
| Message ID descodificado | 14 |
| Station ID descodificado | 9001 |
| Reference time descodificado | 123456 |
| Número de contentores | 1 |
| ID do contentor | 2 — Originating RSU |
| Round-trip e comparação dos campos | PASS |
| Rejeição de payload vazio | PASS |
| Rejeição de payload truncado (último byte removido) | PASS |
| Estado final | `CPM_R2_ROUNDTRIP_PROBE=PASS` |

O teste versionado em `scratch/test/Green/CPM/test_cpm_r2_roundtrip_probe.cc` reproduz a mensagem mínima validada. O artefacto local usado na execução apresentou o SHA-256 `5d2058063ed242cfa828fce6d9ec93a0d5f346d78cb57ab5be05370f9e00d30e`; o log de runtime apresentou `66ee4b82c6b99b6ee604e75f02c590d85a4f5d99c498525d2b3f27f7c5b79c69`.

### Diagnóstico do primeiro link probe

O probe inicial compilava, mas terminava com exit code 134 ao tentar executar `encode()` numa instância CPM vazia. A exceção indicava falha de codificação de `TimestampIts`. A causa foi a ausência de inicialização do conteúdo ASN.1 obrigatório no probe, não uma falha de ligação das bibliotecas. O probe inicial não é classificado como teste positivo; foi substituído, para efeitos de validação do codec, pelo round-trip com estrutura devidamente inicializada.

## Reexecução

No workspace ns-3.47 com a instalação Vanetza R2 presente, executar:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic \
    -I/usr/local/include \
    scratch/test/Green/CPM/test_cpm_r2_roundtrip_probe.cc \
    -L/usr/local/lib -Wl,-rpath,/usr/local/lib \
    -lvanetza_asn1_its_r2 -lvanetza_asn1 -lvanetza_asn1_support \
    -o /tmp/test_cpm_r2_roundtrip_probe

/tmp/test_cpm_r2_roundtrip_probe
```

Asserções principais esperadas:

```text
CPM_ASN1_CONSTRAINTS=PASS
CPM_UPER_ENCODE=PASS
CPM_UPER_SIZE=30
CPM_HEADER_VERSION=2
CPM_MESSAGE_ID=14
CPM_STATION_ID=9001
CPM_REFERENCE_TIME=123456
CPM_CONTAINER_COUNT=1
CPM_CONTAINER_ID=2
CPM_VALID_ROUNDTRIP=PASS
CPM_EMPTY_PAYLOAD_REJECTED=PASS
CPM_TRUNCATED_PAYLOAD_REJECTED=PASS
CPM_R2_ROUNDTRIP_PROBE=PASS
```

## Limites e próximo gate

Esta evidência **não** valida:

- `PerceivedObjectContainer`, `PerceivedObject`, objetos ou sensores reais;
- consistência semântica dos dados de perceção oriundos de SUMO;
- envio/receção por BTP, GeoNetworking, Access Adapter ou `PacketContext`;
- segurança, assinatura, PKI, interoperabilidade externa ou conformidade ETSI.

Nota de correção: segundo a ETSI TS 103 324 V2.1.1, cláusula 7.1.8.1, `numberOfPerceivedObjects` representa o total de objetos percebidos na LDM no instante de geração. Não tem de ser igual à cardinalidade dos objetos incluídos no CPM. A prova isolada posterior confirmou ainda que a API ASN.1 instalada não rejeita automaticamente a ausência de `objectId` nem todos os casos semanticamente inválidos de contagem. Estes casos exigem validação semântica adicional. Nenhuma implementação de produção foi alterada.
