---
title: "Etapa 10.9 — Receção e entrega CPM"
status: "PASS_WITH_SCOPE_LIMITS"
type: "EVIDENCE AND VALIDATION REPORT"
date: 2026-10-26
project: "ns-3.47 + Vanetza + SUMO V2X Integration"
branch: "feature/cpm-etapa10-9"
---

# Etapa 10.9 — Receção e entrega CPM

## 1. Objetivo e estado

Esta etapa integra a receção CPM no runtime Vanetza, descodifica e valida
semanticamente a mensagem, mantém a representação neutra da aplicação e
entrega as mensagens validadas através de uma fila consumida pelo
`SimulationController`.

Estado do fecho: **PASS_WITH_SCOPE_LIMITS**.

O incremento de implementação da receção e entrega CPM fica encerrado
com os testes locais e a demonstração E2E de curta distância registados
neste relatório. Nesta configuração próxima foram observadas 92 receções
e 92 entregas à aplicação.

Na posição original da RSU, apesar de 150 resultados CPM TX aceites,
não foram observados eventos de receção PHY nem entregas à aplicação.
Esta limitação é aceite como condição explicitamente registada para o
fecho deste incremento; não significa que a causa esteja identificada,
que o alcance rádio tenha sido caracterizado ou que o cenário original
esteja validado.

A análise da ausência de receção na posição original permanece um item
de seguimento caso esse cenário seja um requisito obrigatório. A
verificação criptográfica da assinatura permanece reservada à Etapa 10.10.

O estado `PROPOSED` do plano global `ETAPA_10_CPM_PLANO.md` não é alterado
por este relatório.

## 2. Implementação integrada

- Dispatcher BTP dedicado para a porta CPM 2009, sem substituir os handlers
  existentes nas portas CAM 2001, DENM 2002 e SPATEM 2004.
- Perfil CPM com ITS-AID 639 e transporte SHB/ITS-G5.
- Recuperação do `PacketContext` original através de `RxBridgeContext`.
- Descodificação UPER e validações semânticas, incluindo perfil AID, tipo
  de estação, identidade do emissor, posição de referência, campos temporais
  e objetos incluídos.
- Rejeição de mensagens duplicadas pelo contexto de replay CPM e rejeição
  quando a fila de receção não consegue aceitar a mensagem.
- `CpmReceptionQueue` limitada a 256 mensagens por veículo.
- Método `VanetzaRuntime::TryReceiveCpm` para consumir mensagens neutras.
- `SimulationController::DrainCpmReceptionQueues`, executado no ciclo de
  polling existente, que consome as mensagens e regista a entrega e os
  objetos CPM.

A verificação criptográfica da assinatura não é demonstrada nesta etapa.
A validação de AID/perfil não equivale a verificar uma assinatura ETSI.
O log do runtime da RSU indica `signer=DUMMY`; a segurança criptográfica
permanece no âmbito da Etapa 10.10.

## 3. Testes de regressão

| Teste | Resultado |
|---|---|
| Pipeline CPM RX comportamental | PASS — 27 verificações |
| Dispatch e consumidor da aplicação | PASS — 6 testes, 13 subtestes |
| Fronteiras de coordenadas CAM | PASS — 3 casos |
| Receção SPATEM com múltiplas interseções | PASS — 1 caso |
| Analisador V2X recuperado da baseline anterior | PASS — 43 testes |
| Build do target de produção | PASS |
| `git diff --check` | PASS |

O teste comportamental verifica entrega de uma mensagem válida, preservação
da semântica neutra e metadados locais, rejeição de payload malformado,
AID inválido, divergência de identidade, repetição exata e fila cheia.
Os testes estruturais não substituem a evidência da simulação E2E.

## 4. Ensaio E2E de curta distância

Configuração do ensaio:

- Cenário: `sumo/current/scenario/georeferenced/vila-nova-de-famalicao/famalicao-demo.sumocfg`.
- Bridge: `sumo/current/integration/v2x_bridge_16.py`.
- Passo SUMO: 100 ms; duração lógica da bridge: 15 s.
- CPM ativo; CAM, DENM e SPATEM desativados.
- RSU `stationId=9001`, inicialmente colocada na posição local do veículo 0.
- Posição inicial do veículo: aproximadamente `(20489.043, 5179.750)` m.
- Auditoria PHY: `V2X_PHY_AUDIT=1`.

| Métrica | Resultado |
|---|---:|
| Resultados CPM `TX_RESULT ACCEPTED` | 150 |
| Eventos PHY TX da RSU | 151 |
| PHY RX BEGIN no veículo | 92 |
| PHY RX END no veículo | 92 |
| PHY RX DROP no veículo | 59 |
| Motivo numérico dos descartes PHY | `reason_code=9` |
| `CPM_RECEPTION_OK` | 92 |
| `CPM_DELIVER_OK` | 92 |
| `[CPM_APP_RX] DELIVER_OK` | 92 |
| `[CPM_APP_RX_OBJECT]` | 92 |

Os logs demonstram que as mensagens recebidas atravessam o callback do
socket, `VanetzaRuntime::ReceivePacket`, a receção CPM e o consumidor da
aplicação. Os campos semânticos do objeto foram observados nos registos
`CPM_APP_RX_OBJECT`.

Os contadores TX, PHY e aplicação são contagens independentes. Não se
assume correspondência um-para-um entre os 150 resultados TX e os 151
eventos PHY TX.

## 5. Ensaio na posição original da RSU

Configuração da RSU:

- Latitude: `41.42528995`
- Longitude: `-8.43407620`
- Coordenadas locais SUMO/ns-3: `X=20374.270`, `Y=9640.360`
- Alcance horizontal de perceção CPM: 7000 m

A distância inicial entre esta RSU e a posição inicial do veículo 0 é
aproximadamente 4,46 km.

| Métrica | Resultado |
|---|---:|
| `TX_RESULT ACCEPTED` | 150 |
| Eventos PHY TX da RSU | 151 |
| PHY RX BEGIN no veículo | 0 |
| PHY RX END no veículo | 0 |
| PHY RX DROP no veículo | 0 |
| Entregas à aplicação | 0 |

Não foi observada receção nesta topologia durante o ensaio. A ausência de
eventos PHY RX não prova por si só um alcance máximo específico; mostra que
a posição original não produziu receção observável neste cenário e nesta
configuração de rádio.

O alcance de perceção CPM de 7000 m define a seleção de objetos percebidos,
não o alcance rádio do enlace 802.11p.

## 6. Sweep de afastamento inicial

O veículo continua a mover-se durante cada execução. A distância abaixo é
o afastamento inicial configurado para a RSU relativamente à posição
inicial do veículo, não uma distância constante durante toda a simulação.

| Afastamento inicial | TX aceites | PHY RX END | PHY RX DROP | Entregas à aplicação |
|---:|---:|---:|---:|---:|
| 75 m | 150 | 92 | 59 | 92 |
| 80 m | 150 | 84 | 68 | 84 |
| 85 m | 150 | 52 | 99 | 52 |
| 90 m | 150 | 24 | 127 | 24 |
| 100 m | 150 | 0 | 151 | 0 |
| 250 m | 150 | 0 | 151 | 0 |
| 500 m | 150 | 0 | 151 | 0 |
| 1000 m | 150 | 0 | 0 | 0 |

Os ensaios de 75 a 90 m mostram uma redução de entregas com o aumento do
afastamento inicial. Os descartes PHY observados usam `reason_code=9`.
Estes resultados não constituem uma estimativa rigorosa de alcance máximo:
a trajetória do veículo e as condições da simulação influenciam a contagem.
Os casos de 1000 m e da posição original não registaram eventos de RX no
veículo, pelo que também não permitem deduzir o limite de sensibilidade.

## 7. Limitações e critérios pendentes

1. A receção funcional e o consumo da aplicação estão demonstrados na
   topologia de curta distância.
2. A receção na posição original da RSU não foi demonstrada.
3. O sweep é exploratório; não mantém a distância constante nem caracteriza
   potência recebida, sensibilidade ou alcance máximo.
4. A assinatura criptográfica não é verificada nesta etapa e permanece
   reservada à Etapa 10.10.
5. Não se declara interoperabilidade ETSI, conformidade global ou validação
   OTA a partir destes ensaios locais.
6. O estado global do plano da Etapa 10 permanece inalterado.

## 8. Artefactos de evidência

Os logs completos e resumos foram arquivados em:

`test-results/cpm_etapa10_9_rx_20261026/`

Subdiretórios:

- `near/`: execução E2E de curta distância com auditoria PHY.
- `original/`: execução com a posição original da RSU.
- `range_refined/`: execuções com afastamento inicial de 75, 80, 85 e 90 m.
- `range_sweep/`: execuções com afastamento inicial de 100, 250, 500 e 1000 m.

O arquivo não inclui artefactos de `sumo/`; estes permanecem fora do
staging e não foram alterados por este procedimento.

## 9. Decisão de fecho

**Decisão:** encerrar o incremento de implementação da Etapa 10.9 com
estado `PASS_WITH_SCOPE_LIMITS`.

O contrato funcional de receção e entrega foi demonstrado na configuração
E2E de curta distância e coberto pelos testes documentados. O resultado
não constitui validação da receção na posição original da RSU, estimativa
de alcance máximo, verificação criptográfica, interoperabilidade ETSI ou
conformidade OTA.

A pendência relativa à posição original permanece explicitamente
documentada para seguimento, caso seja necessária para os critérios do
cenário de integração.
