# Etapa 11.5.2 — SUMO Person Ingress (RED)

## Objetivo

Especificar a fronteira de ingestão de pessoas do SUMO sem transformar uma pessoa numa `VehicleState` e sem criar ainda nós ns-3, identidades ITS, GN addresses ou endpoints VAM.

## Contrato de transporte proposto

O bridge publica datagramas UDP separados e explicitamente discriminados:

```text
SUMO_PERSON,UPSERT,<personId>,<x>,<y>,<speed>,<angleDeg>,<simulationTimeUs>
SUMO_PERSON,REMOVE,<personId>,<simulationTimeUs>
```

- `personId` é a identidade lógica SUMO da pessoa. Não é `nodeId`, `stationId`, GN_ADDR.MID nem MAC.
- `x` e `y` são coordenadas cartesianas SUMO em metros. Não são WGS84.
- `speed` é m/s e tem de ser finita e não negativa.
- `angleDeg` preserva o ângulo TraCI em graus; o parser não faz conversão de referencial.
- `simulationTimeUs` é obrigatório, inteiro `uint64` e tempo lógico SUMO. Não é tempo ITS nem wall-clock.
- `UPSERT` é enviado para cada pessoa ativa em cada passo. A primeira amostra permite criar a entrada; as seguintes atualizam-na.
- `REMOVE` é explícito para IDs presentes no passo anterior e ausentes no atual. Ausência de um datagrama nunca é, por si só, um evento de remoção.
- Todos os registos de pessoas têm de preceder `SUMO_STEP_END` do mesmo passo.
- Quando o ciclo de simulação termina (por ausência de entidades esperadas ou pelo limite temporal), o bridge tem de reconciliar o snapshot final: emitir `REMOVE` para cada pessoa ainda registada e enviar um `SUMO_STEP_END` final. Não se prolonga o ciclo principal com `or previous_person_ids`, evitando uma extensão ilimitada quando existe um limite temporal.
- O prefixo `SUMO_PERSON,` tem de ser consumido pelo caminho próprio do `SumoAdapter` antes do fallback para `SumoParser` de veículos. Um datagrama de pessoa, mesmo inválido, nunca pode ser interpretado como veículo.

A publicação de atualizações completas por pessoa em cada passo permite recuperar uma atualização perdida no passo seguinte. UDP continua a não oferecer entrega garantida; a semântica de remoção é explícita, mas esta etapa não introduz ACK/retransmissão fiável.

## Validação do parser

Rejeitar: prefixo/tipo desconhecido, aridade inválida, ID vazio, campos numéricos vazios ou não finitos, velocidade negativa, os valores sentinela TraCI para posição/velocidade/ângulo, timestamp ausente, negativo, fracionário ou fora de `uint64` e campos extra.

Para `REMOVE`, a amostra de mobilidade tem de estar ausente. Para `UPSERT`, a amostra tem de estar presente.

## Fronteiras

Incluído nesta etapa: serializer/publicador Python, parser C++, callback dedicado no `SumoAdapter` e testes de encaminhamento.

Fora desta etapa: criação de `Node`, `MobilityModel`, `ItsStationState`, `stationId`, certificados, runtime Vanetza, classificação/política VRU, VAM/POTI e transmissão VAM. O callback fica disponível como fronteira de ingestão para o consumidor VAM posterior.

`traci.simulation.getMinExpectedNumber()` já contabiliza veículos e pessoas ativos e ainda por iniciar; não é necessário alterar a condição de continuação do ciclo principal.

## Execução RED

Na raiz `~/final/ns-3.47`:

```bash
python3 sumo/current/integration/tests/test_v2x_bridge_16_person.py
python3 scratch/test/Red/SUMO/test_sumo_person_datagram.py
python3 scratch/test/Red/SUMO/test_sumo_person_adapter_wiring.py
```

Os testes são intencionalmente RED neste commit de testes: os métodos Python, parser e callback específicos ainda não existem. Nenhum ficheiro de produção é alterado por este patch.
