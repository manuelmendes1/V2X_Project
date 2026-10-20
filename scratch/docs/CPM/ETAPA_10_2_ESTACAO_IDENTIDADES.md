# Etapa 10.2 — Estação de origem, identidades e perfil CPM R2

**Estado:** decisão documentada; gate sujeito a revisão
**Data da evidência:** 2026-10-20
**Perfil:** ETSI TS 103 324 V2.1.1 (2023-06), Release 2
**Branch:** `feature/cpm-etapa10-2-perceived-object`

## 1. Perfil da estação

O cenário emissor inicial é uma Vehicle ITS-S.

- `protocolVersion = 2` e `messageId = 14` identificam o perfil CPM R2 utilizado no probe.
- A mensagem usa `OriginatingVehicleContainer`, com `containerId = 1`.
- `orientationAngle` é um `Wgs84Angle`, com valor e confiança.
- O fixture usa `stationId = 9001`, orientação sintética `value = 0` e `confidence = 127`.
- Os valores sintéticos provam a estrutura ASN.1 e o round-trip, não a orientação física de um veículo.

Referência: ETSI TS 103 324 V2.1.1, cláusula 7.1.4.

## 2. Sistema de coordenadas

Para uma Vehicle ITS-S, as variáveis de estado dos objetos percebidos usam um referencial East-North-Up (ENU) centrado na posição de referência do veículo, conforme a cláusula 7.1.8.1.

Este referencial não deve ser confundido com o referencial solidário com o veículo usado na descrição da região de perceção dos sensores.

O probe atual usa coordenadas sintéticas. Não demonstra a transformação de coordenadas SUMO para o referencial CPM; essa conversão requer testes próprios.

## 3. Separação de identidades

| Identificador | Semântica | Regra |
|---|---|---|
| `stationId` | Identidade da estação no ITS PDU Header | Não derivar do ID de objeto nem do endereço de rede |
| `GN_ADDR` | Endereço GeoNetworking | Preservar a autoridade de identidade definida pela integração Vanetza |
| MAC | Endereço da camada de acesso | Não tratar como `GN_ADDR` nem como `stationId` |
| `packetId` | Identidade do pacote no tracing do projeto | Preservar a continuidade de `PacketContext` |
| `objectId` | Identidade protocolar de um objeto percebido | Gerir conforme a cláusula 7.1.8.2 |
| ID TraCI | Identificador interno da entidade SUMO | Não atribuir diretamente como `objectId` por conveniência |

Estes identificadores não são intercambiáveis. O valor de teste `objectId = 501` é uma fixture determinística e não define o allocator de produção.

## 4. Ciclo de vida de objectId

A cláusula 7.1.8.2 da ETSI TS 103 324 V2.1.1 define a gestão dos identificadores de objetos percebidos.

- O domínio de `objectId` é `0..65535`.
- A identidade mantém-se associada ao objeto enquanto este continuar a ser percebido e não ocorrer uma mudança de pseudónimo.
- Um novo objeto recebe um identificador disponível, de acordo com as regras normativas de atribuição.
- Um identificador utilizado não deve ser reutilizado antes do fim do período de retenção aplicável.
- Após uma mudança de pseudónimo, os identificadores atuais dos objetos são substituídos de acordo com as regras da norma.

O allocator de produção fica fora do âmbito do probe. A implementação deverá gerir atribuição, associação, retenção e mudança de pseudónimo. Os IDs TraCI podem apoiar a associação interna, mas não substituem a semântica protocolar de `objectId`.

## 5. Semântica de numberOfPerceivedObjects

A cláusula 7.1.8.1 define `numberOfPerceivedObjects` como o total de objetos percebidos na LDM no instante de geração do CPM. Devido às regras de geração e inclusão, este total não tem de ser igual ao número de objetos incluídos na mensagem.

Para o cenário base, sem clustering, a validação semântica não deve impor igualdade entre esses números. Deve, contudo, rejeitar um total declarado inferior ao número de objetos transmitidos. Cenários com grupos ou clusters de VRUs terão de respeitar a semântica própria definida pela norma.

Os testes demonstraram que as constraints ASN.1 instaladas não rejeitam automaticamente todos os casos semanticamente inválidos. A presença de `objectId` e a coerência da contagem exigem validação semântica adicional.

## 6. Evidência isolada

Runner: `python3 scratch/test/Green/CPM/test_cpm_r2_perceived_object_probe.py -v`

Resultados observados em 2026-10-20:

- Compilação e execução: PASS, código de saída `0`.
- Round-trip UPER com `OriginatingVehicleContainer`: PASS.
- Objeto de teste com ID `501`, delta `-12`, coordenadas `(1200, -350)` e qualidade `12`: PASS.
- Payload vazio e payload truncado rejeitados: PASS.
- Ausência de `objectId` rejeitada pelo guard semântico: PASS.
- Total declarado `2` com um objeto incluído aceite: PASS.
- Total declarado `0` com um objeto incluído rejeitado: PASS.
- Matriz de 24 fronteiras numéricas: PASS.
- `git diff --check`: PASS.

O tamanho observado de 50 bytes pertence apenas ao fixture executado. Não é um limite normativo nem um tamanho fixo de CPM.

Esta evidência não valida o caminho TX/RX de produção, BTP/GeoNetworking, security/replay, `PacketContext`, analyzer, integração SUMO ou interoperabilidade.

## 7. Critérios da decisão

- [x] Perfil inicial Vehicle ITS-S escolhido.
- [x] Separação conceptual dos identificadores documentada.
- [x] Regras de ciclo de vida de `objectId` documentadas.
- [x] Semântica de `numberOfPerceivedObjects` corrigida.
- [x] Prova isolada do codec e das fronteiras disponível.
- [ ] Revisão final e aceitação do gate da Fase 10.2.

Referência normativa:
ETSI TS 103 324 V2.1.1 — Collective Perception Service (Release 2):
https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf
