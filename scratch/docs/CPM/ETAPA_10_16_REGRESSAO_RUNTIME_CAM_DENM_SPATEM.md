---
title: "Etapa 10.16 — Regressão runtime CAM, DENM e SPATEM"
status: "PARTIAL_OPEN"
integration_status: "NOT_CLOSED"
type: "RUNTIME REGRESSION EVIDENCE REPORT"
date: "2026-11-02"
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
branch: "feature/cpm-etapa10-16-regression-cam-denm-spatem"
validation_head: "1ab9e4ab5fffc29bad25265a1f377cf08f0f7584"
scope: "CAM / DENM / SPATEM runtime coexistence"
---

# Etapa 10.16 — Regressão runtime CAM, DENM e SPATEM

## 1. Estado e decisão

Esta etapa regista a campanha runtime de regressão dos três serviços CAM,
DENM e SPATEM na revisão `1ab9e4ab5fffc29bad25265a1f377cf08f0f7584`, na
branch `feature/cpm-etapa10-16-regression-cam-denm-spatem`.

**Estado: `PARTIAL_OPEN` — a etapa não está fechada.**

Foram executados os sete cenários da matriz: três serviços isolados, três
combinações aos pares e a combinação tripla. Os três cenários isolados e as
três combinações aos pares demonstraram entrega funcional dos serviços
ativos. A combinação tripla transmitiu CAM e DENM, mas não demonstrou entrega
desses dois serviços; SPATEM manteve receção, descodificação e entrega.

A falha da combinação tripla é o único bloqueio funcional observado nesta
matriz. Não se declara aprovação global da coexistência dos três serviços.

## 2. Âmbito e procedimento

O âmbito desta campanha é a regressão runtime dos serviços CAM, DENM e
SPATEM no executável de produção `build/scratch/ns3.47-v2x_sumo-default`.
Os ensaios usaram `--simDuration=45`, o adaptador UDP em `127.0.0.1:7400` e
`V2X_PHY_AUDIT=1`.

Os injectors da campanha phase 9.15 foram reutilizados para gerar estados
veiculares e, nos cenários DENM, um único evento DENM. Nos cenários SPATEM,
foi usado o publisher SUMO/TraCI com o cenário `rilsa3_tls_only.sumocfg`,
TLS `0`, RSU `stationId=9001` e interseção `42`. O publisher reportou 5040
registos TLS e quatro fases observadas nos cenários CAM+SPATEM e SPATEM-only.

As contagens representam eventos de log independentes. Não se pressupõe
correspondência um-para-um entre transmissões, receções e entregas, em
particular para SPATEM, cuja transmissão pode ser recebida por vários
veículos.

## 3. Matriz runtime consolidada

| Cenário | CAM TX / receção / entrega | DENM eventos / TX / entrega | SPATEM TX / BTP RX / decode / entrega | PHY RX drops |
|---|---:|---:|---:|---:|
| CAM-only | 88 / 85 / 85 | — | — | 0 |
| DENM-only | — | 1 / 4 / 1 | — | 0 |
| SPATEM-only | — | — | 55 / 98 / 98 / 98 | 31 |
| CAM + DENM | 88 / 84 / 84 | 1 / 4 / 1 | — | 4 |
| CAM + SPATEM | 88 / 76 / 76 | — | 56 / 86 / 86 / 86 | 102 |
| DENM + SPATEM | — | 1 / 4 / 1 | 57 / 106 / 106 / 106 | 74 |
| CAM + DENM + SPATEM | 88 / 0 / 0 | 1 / 4 / 0 | 58 / 93 / 93 / 93 | 212 |

### 3.1 CAM

Nos cenários CAM-only, CAM+DENM e CAM+SPATEM, foram observadas 88 gerações
e 88 transmissões concluídas em cada cenário. As entregas foram,
respetivamente, 85, 84 e 76. No cenário triplo, as 88 transmissões foram
concluídas, mas não foram observadas `CAM_RECEPTION_OK` nem `CAM_DELIVER_OK`.

No CAM-only, os 85 eventos de receção atravessaram `RAW_RX`, `ACCESS_RX_OK`,
BTP porta 2001 e descodificação CAM, chegando à entrega na aplicação.
A execução CAM-only apresentou `VERIFY_PERFORMED=0`; não demonstra
verificação criptográfica CAM.

### 3.2 DENM

Cada cenário que ativa DENM ingeriu um evento. O evento produziu quatro
mensagens transmitidas (mensagem inicial e repetições), todas com
`TX_COMPLETE=OK`.

Foi observada uma entrega DENM em DENM-only, CAM+DENM e DENM+SPATEM. A linha
de entrega DENM-only apresenta `VERIFY_PERFORMED=1`, `VERIFY_OK=1` e
`VERIFY_AID=37`. O cenário triplo não apresenta `DENM_DELIVER_OK`, apesar
das quatro transmissões concluídas.

As quatro transmissões não representam quatro eventos independentes nem
quatro entregas.

### 3.3 SPATEM

Nos quatro cenários com SPATEM, a campanha observou `TX_COMPLETE`, receção
BTP na porta 2004, descodificação SPATEM e entrega. Os pares de contagens
TX/entrega foram 55/98 (SPATEM-only), 56/86 (CAM+SPATEM), 57/106
(DENM+SPATEM) e 58/93 (combinação tripla).

Nos ensaios com publisher TLS, o publisher reportou 5040 registos enviados,
quatro fases observadas e `TRACI_SUMO_TLS_PUBLISHER=PASS`.

A evidência demonstra o percurso funcional SPATEM nesta simulação. Não
constitui prova de assinatura SPATEM com PKI de produção ou de
interoperabilidade externa.

## 4. Descartes PHY

Os contadores `PHY_RX_DROP` por cenário foram 0 (CAM-only), 0 (DENM-only),
31 (SPATEM-only), 4 (CAM+DENM), 102 (CAM+SPATEM), 74 (DENM+SPATEM) e 212
(CAM+DENM+SPATEM).

Para o cenário CAM+SPATEM, os 102 eventos foram classificados pelo enum
`WifiPhyRxfailureReason` do ns-3.47:

| Código | Símbolo ns-3.47 | Ocorrências |
|---:|---|---:|
| 4 | `TXING` | 22 |
| 8 | `BUSY_DECODING_PREAMBLE` | 10 |
| 9 | `PREAMBLE_DETECT_FAILURE` | 70 |
| | **Total** | **102** |

Distribuição observada por papel recetor:

| Papel | Código 4 | Código 8 | Código 9 | Total |
|---|---:|---:|---:|---:|
| RSU | 11 | 0 | 50 | 61 |
| VEHICLE | 11 | 10 | 20 | 41 |

A correspondência dos códigos foi conferida pela ordem e pelos nomes da
enumeração `WifiPhyRxfailureReason` em
[ns-3.47 `wifi-phy-common.h`](https://fossies.org/linux/ns-allinone/ns-3.47/src/wifi/model/wifi-phy-common.h).

O código 9 foi o motivo mais frequente neste cenário. Os eventos de PHY são
observações do trace de receção e não identificam, por si só, o serviço
responsável pelo pacote nem demonstram causalidade entre SPATEM e a perda de
CAM. Também não se deve equiparar número de drops a número de mensagens V2X
perdidas.

Foram observados 18 eventos sem `packet_id` no cenário CAM+SPATEM. O auditor
só escreve esse campo quando encontra `PacketIdTag` no objeto `Packet` do
trace `PhyRxDrop`; `packet_id=missing` significa que a tag não foi encontrada,
não que o objeto `Packet` seja necessariamente nulo.

## 5. Resultado por cenário

| Cenário | Decisão | Fundamentação |
|---|---|---|
| CAM-only | PASS | 85 entregas CAM; serviços DENM e SPATEM ausentes. |
| DENM-only | PASS | 1 evento, 4 TX concluídos e 1 entrega com verificação válida (`VERIFY_OK=1`, AID 37). |
| SPATEM-only | PASS | 55 TX concluídos e 98 entregas SPATEM; CAM e DENM ausentes. |
| CAM + DENM | PASS | 84 entregas CAM e 1 entrega DENM verificada. |
| CAM + SPATEM | PASS_WITH_PHY_DROPS | 76 entregas CAM e 86 entregas SPATEM; 102 drops PHY observados. |
| DENM + SPATEM | PASS_WITH_PHY_DROPS | 1 entrega DENM e 106 entregas SPATEM; 74 drops PHY observados. |
| CAM + DENM + SPATEM | FAIL_PARTIAL | SPATEM entregue (93); CAM sem receção/entrega e DENM sem entrega; 212 drops PHY observados. |

As decisões `PASS_WITH_PHY_DROPS` significam que os percursos de aplicação
foram demonstrados apesar dos eventos PHY observados. Não significam ausência
de perdas ou garantia de entrega.

## 6. Integridade, builds e verificações fora da matriz

- `git diff --check`: PASS no fecho da campanha.
- Build limpo do target `ns3.47-v2x_sumo-default`: PASS na validação
  anterior à campanha.
- O simulador, os injectors e o publisher terminaram com código de saída 0
  nos ensaios em que foram executados.
- A suite `python3 scratch/test/run_tests.py --suite all` tinha 96 testes,
  com 1 falha e 0 erros. A falha registada foi o E2E sintético CPM que terminou
  sem entrega (`simulation ended without delivery`). Esta falha não é
  convertida em PASS pela matriz CAM/DENM/SPATEM.
- Os testes do analisador V2X não foram executados nesta validação, devido
  ao ambiente Python local sem `pytest` disponível e ao virtualenv com
  interpretador referenciado em falta. Não classificar este caso como teste
  aprovado nem como falha de código.
- O caminho `--enableCpm` no entrypoint de produção apenas inicializa a
  construção de `CpmInput`; não demonstra transmissão CPM runtime. CPM não
  faz parte da matriz funcional demonstrada por este relatório.

## 7. Limites da evidência

1. Os resultados são de simulação local ns-3.47, Vanetza e SUMO/TraCI.
2. Não foi demonstrada interoperabilidade com equipamento ITS externo,
   hardware ITS-G5 ou conformidade formal integral ETSI.
3. A validação de segurança criptográfica observada nesta matriz é específica
   da entrega DENM indicada. CAM não teve verificação criptográfica
   demonstrada e SPATEM não teve verificação de assinatura de produção
   demonstrada.
4. A classificação PHY detalhada foi obtida para CAM+SPATEM. Para os outros
   cenários, este relatório conserva apenas o total de eventos `PHY_RX_DROP`.
5. A matriz não identifica ainda a causa da ausência de receção/entrega CAM e
   DENM na combinação tripla.
6. Os logs completos estão no diretório local `test-results/` e não são
   incorporados por este relatório. Os caminhos abaixo identificam os
   diretórios de evidência gerados nesta campanha; a sua presença no
   checkout local deve ser preservada separadamente.

## 8. Artefactos de evidência

Raiz local:

`test-results/cpm_etapa10_16_runtime_20261102/`

| Cenário | Diretório |
|---|---|
| CAM-only | `cam-only.SN68GF/` |
| DENM-only | `denm-only.VDfyNJ/` |
| SPATEM-only | `spatem-only.AxlvQQ/` |
| CAM + DENM | `cam-denm.Ur2Cok/` |
| CAM + SPATEM | `cam-spatem.pJfPYm/` |
| DENM + SPATEM | `denm-spatem-retry.wZyRk6/` |
| CAM + DENM + SPATEM | `cam-denm-spatem.rvwbs5/` |

Os diretórios contêm os logs `ns3.log` e, quando aplicável, os logs dos
injectors e do publisher TLS. Os logs são artefactos locais da execução e
não são adicionados automaticamente ao commit documental.

## 9. Pendência e critério de fecho

**Pendência aberta:** explicar e corrigir, ou delimitar formalmente, a ausência
de receção/entrega CAM e de entrega DENM quando CAM, DENM e SPATEM estão
ativos em simultâneo.

Critérios mínimos para reavaliar o fecho:

- reproduzir o cenário triplo com a mesma configuração e conservar os logs;
- demonstrar receção e entrega CAM e DENM, ou documentar uma limitação aceite
  com causa e âmbito comprovados;
- preservar as regressões isoladas e aos pares já aprovadas;
- repetir a matriz relevante após qualquer alteração funcional e comparar
  contagens TX, RX, entrega e drops PHY separadamente.

**Decisão final:** manter a Etapa 10.16 em `PARTIAL_OPEN`. Os seis cenários
isolados e aos pares estão funcionalmente demonstrados; a combinação tripla
continua a impedir o encerramento global. Não declarar a matriz completa como
PASS até essa pendência ser resolvida ou formalmente aceite com evidência.
