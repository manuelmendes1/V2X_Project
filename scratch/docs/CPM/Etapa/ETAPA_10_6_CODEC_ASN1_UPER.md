---
title: "Etapa 10.6 — Codec ASN.1 / UPER"
status: "ACTIVE"
type: "FUNCTIONAL DEVELOPMENT AND VALIDATION REPORT"
date: 2026-10-23
project: "ns-3.47"
stage: "10.6"
branch: "feature/cpm-etapa10-6-codec"
tags:
  - CPM
  - ASN.1
  - UPER
  - Vanetza
---

# Etapa 10.6 — Codec ASN.1 / UPER

## 1. Estado

**ACTIVE — perfil CPM R2 de origem RSU implementado e validado.**

Foi acrescentado um decoder que transforma uma mensagem UPER CPM R2 num
`CpmMessage`. O encoder escreve `knownObjectCount` no campo ASN.1
`numberOfPerceivedObjects`, e o decoder preserva esse valor. A contagem
transmitida pode ser superior ao número de objetos incluídos na lista.

A build limpa e a suite CPM passaram. A implementação cobre um perfil restrito,
não todos os contentores e campos opcionais da edição CPM R2. Não se declara
conformidade integral nem interoperabilidade externa.

O plano global `ETAPA_10_CPM_PLANO.md` permaneceu inalterado.

## 2. Ficheiros alterados ou adicionados

- `scratch/vanetza_integration/cpm/codec/cpm_uper_decoder.h`
- `scratch/vanetza_integration/cpm/codec/cpm_uper_decoder.cc`
- `scratch/vanetza_integration/cpm/codec/cpm_uper_encoder.cc`
- `scratch/vanetza_integration/cpm/types/cpm_domain_types.h`
- `scratch/CMakeLists.txt`
- `scratch/test/Red/CPM/test_cpm_uper_decoder.cc`
- `scratch/test/Red/CPM/test_cpm_uper_decoder.py`

Foi também criada uma allowlist específica no `.gitignore` para este relatório.

## 3. Contrato do decoder

A API pública é `CpmUperDecoder::Decode(payload, receivedAtUnixMs)`.

O decoder utiliza o descritor ASN.1 CPM R2 gerado pelo Vanetza e `uper_decode`.
Após a descodificação, verifica o resultado, as restrições ASN.1 e a quantidade
de bits consumidos. Rejeita bytes adicionais depois do PDU e padding não zero
no último octeto.

Em caso de erro, o resultado não contém um `CpmMessage` parcial e apresenta
uma mensagem de erro.

`receivedAtUnixMs` é metadado local fornecido pelo recetor; não é retirado
do conteúdo CPM recebido.

## 4. Perfil semântico suportado

O perfil validado inclui:

- Cabeçalho com `protocolVersion = 2`, `messageId = cpm` e `stationId` válido.
- Origem RSU sem `mapReference`.
- `referenceTime` não negativo.
- Posição de referência WGS84, com os componentes de altitude e confiança
  indisponíveis, de acordo com o perfil implementado.
- Um contentor de origem RSU e um contentor de objetos percecionados.
- Uma lista de objetos que pode estar vazia; a contagem transmitida pode
  exceder a lista, mas não pode ser inferior à quantidade incluída.
- Objetos com `objectId` não zero, `measurementDeltaTime`, posição cartesiana
  horizontal, confiança das coordenadas indisponível e `objectAge` opcional.
- Rejeição de contentores duplicados, ausentes, desconhecidos ou com CHOICE
  inconsistente.
- Rejeição de campos opcionais de objetos que o perfil ainda não representa.
- Rejeição dos códigos reservados ou valores inválidos explicitamente cobertos
  pelos testes negativos.

A rejeição explícita evita produzir uma mensagem semântica aparentemente válida
quando a estrutura recebida contém dados que o modelo atual não consegue
representar fielmente.

## 5. Semântica de `numberOfPerceivedObjects`

No CPM R2, `numberOfPerceivedObjects` descreve o total de objetos
percecionados conhecidos pelo ITS-S emissor no LDM no momento da geração
da mensagem. Não tem de ser igual ao número de objetos incluídos na lista
`perceivedObjects`.

Referência normativa: ETSI TS 103 324 V2.1.1, cláusula 7.1.8.1:
https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf

O campo semântico `knownObjectCount` é mapeado pelo encoder para
`numberOfPerceivedObjects`. O decoder lê esse campo para `knownObjectCount`;
não o infere a partir da lista incluída.

O modelo usa atualmente um `std::uint8_t` não opcional. Assim, o valor `0`
não representa um estado explícito de "desconhecido": o produtor tem de
fornecer uma contagem coerente com o seu estado local.

A validação cobre:
- Roundtrip de `knownObjectCount = 7` com um objeto incluído.
- Fixture ASN.1 fixa com contagem transmitida igual a `2` e um objeto incluído.
- Rejeição quando a contagem transmitida é inferior à lista incluída.
- Aceitação de uma lista vazia, mantendo os contentores obrigatórios do perfil.

## 6. Fixture UPER fixa

Foi construída uma fixture de 49 bytes diretamente com as estruturas ASN.1 R2
geradas pelo Vanetza e validada com `asn_check_constraints`. A sua construção
não utiliza o `CpmUperEncoder` de produção.

Origem: `Direct_ASN1_R2_Construction`

Tamanho: `49` bytes

HEX: `020e00002329000000078902727e530332189e07ffffff08eddd0f9101004110100500800fabfa204b0fff7fa8bffcc800`

Validação ASN.1 durante a construção: `PASS`.

A fixture verifica a preservação do cabeçalho, `referenceTime`, posição WGS84,
estados de altitude e confiança indisponíveis, `knownObjectCount` distinto
da cardinalidade da lista incluída, identidade e tempo do objeto, posição,
`objectAge` e timestamp local de receção.

A fixture é independente do encoder de produção, mas continua a usar as
estruturas ASN.1 geradas e as bibliotecas Vanetza instaladas. Não é uma captura
externa nem demonstra interoperabilidade com uma implementação independente.

## 7. Evidência de testes

### Decoder e fixture fixa

Comando: `python3 scratch/test/Red/CPM/test_cpm_uper_decoder.py`

Resultado registado:

- `CPM_GOLDEN_FIXTURE_CONTRACT=PASS`
- `CPM_UPER_DECODER_CASES=61`
- `CPM_UPER_DECODER_PASSED=61`
- `CPM_UPER_DECODER_FAILED=0`
- `CPM_UPER_DECODER_CONTRACT=PASS`

A matriz de testes cobre payload vazio, curto, truncado ou com octeto
adicional; padding UPER não zero; versão e `messageId` incompatíveis; longitude
`valueNotUsed`; coordenada cartesiana reservada; `objectId` ausente; campo
opcional fora do perfil; contagem inferior à lista; contentores RSU ou de
objetos ausentes ou duplicados; contentor não suportado; e `CHOICE` inconsistente.

O valor `CPM_UPER_DECODER_CASES=61` representa asserções individuais de teste,
incluindo verificações de criação de fixtures, não 60 payloads distintos.

### Encoder

Comando: `python3 scratch/test/Red/CPM/test_cpm_uper_encoder.py`

Resultado registado: `CPM_UPER_ENCODER_CONTRACT=PASS`.

As regressões do perfil RSU, tamanho UPER, cabeçalho, `referenceTime`, posição
geográfica, contentores e campos de objeto continuaram verdes.

### Build limpa

Foi executada a sequência:

- `./ns3 clean`
- `./ns3 configure`
- `./ns3 build`

Resultado registado: compilação e ligação concluídas em `[1113/1113]`.

A build posterior do alvo `ns3.47-v2x_sumo-default` também passou.

### Suite CPM completa

A suite Python sob `scratch/test/Green/CPM` e `scratch/test/Red/CPM` foi
executada depois das alterações finais.

Resultado registado:

- `CPM_TESTS_TOTAL=20`
- `CPM_TESTS_FAILED=0`
- `CPM_ETAPA10_6_SUITE=PASS`
- `GIT_DIFF_CHECK=PASS`

## 8. Limites e trabalho restante

A implementação não cobre toda a edição CPM R2. Permanecem fora do perfil,
entre outros:

- `segmentationInfo` e `messageRateRange`;
- contentores de informação de sensores e regiões de perceção;
- `mapReference` e `mapPosition`;
- velocidade, aceleração, ângulos e velocidade angular;
- matrizes de correlação, dimensões, qualidade de perceção, listas de sensores
  e classificações;
- valores conhecidos de altitude e confiança para os quais ainda não existe
  conversão inversa sem perda semântica.

O caso negativo de padding UPER não zero foi executado e passou na suite do decoder.

Não foi demonstrada a integração deste decoder numa pipeline de receção
de CPM nem interoperabilidade com bytes capturados de outro stack. Estes
pontos não são declarados concluídos nesta etapa.

## 9. Conclusão

O perfil CPM R2 RSU implementado tem encoder, decoder semântico, fixture fixa,
rejeição dos casos negativos auditados e regressões automatizadas verdes.

O estado permanece `ACTIVE`: a cobertura do codec é restrita, o modelo não
tem um estado explícito para uma contagem global desconhecida e não existe
evidência de integração numa pipeline de receção nem de interoperabilidade
externa. O relatório não altera o plano global da Etapa 10.

## Evidência da validação semântica da contagem

O teste de contagem inferior à lista utiliza um PDU com dois objetos incluídos
e `numberOfPerceivedObjects = 1`. O valor declarado respeita o domínio ASN.1,
mas é inferior à cardinalidade da lista, exercitando a rejeição semântica.
