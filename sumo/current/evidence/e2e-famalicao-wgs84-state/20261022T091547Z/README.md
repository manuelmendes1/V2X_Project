# Evidência E2E — SUMO WGS84 para ns-3

Data: 2026-10-22
Estado: PASS — cobertura integral das amostras de estado WGS84 nesta execução.

## Ambiente e configuração

- SUMO: 1.25.0, via TraCI.
- Cenário: `sumo/current/scenario/georeferenced/vila-nova-de-famalicao/famalicao-demo.sumocfg`.
- Bridge: `sumo/current/integration/v2x_bridge_16.py`.
- Binário ns-3: `build/scratch/ns3.47-v2x_sumo-default`.
- Receção: UDP 7400.
- TLS selecionado explicitamente: `11645226075`.
- Duração ns-3: 40 s; duração do bridge SUMO: 15 s.
- O log de progresso por passo do SUMO foi desativado para manter os registos do bridge em linhas separadas.

## Contrato validado

O datagrama de estado tem nove campos:
`vehId,x,y,speed,heading,accel,simulationTimeUs,longitudeDeg,latitudeDeg`.

- `x/y` permanecem nas coordenadas projetadas da rede SUMO.
- `simulationTimeUs` é o tempo lógico SUMO em microssegundos.
- Longitude e latitude são fornecidas por `traci.simulation.convertGeo(x, y)`.
- Em caso de falha ou coordenadas WGS84 inválidas, o bridge recua para sete campos; não inventa coordenadas.

## Resultado

Ver `correlation-results.txt` para os contadores recalculados diretamente dos logs brutos.

A correlação utiliza o par `(vehId, simulationTimeUs)` e compara longitude/latitude do datagrama com os valores registados pelo diagnóstico `VEHICLE_STATE_GEODETIC_APPLIED`, emitido depois de `VehicleStateUpdater::Apply`.

A verificação adicional de round-trip TraCI, realizada durante o preflight desta execução do cenário, apresentou erro de 0,0068 m ao converter a posição de rede para WGS84 e novamente para coordenadas de rede. Este valor é uma verificação de consistência da conversão, não uma comparação com um marco geodésico independente.

## Limites

Esta evidência valida o transporte e a persistência de WGS84 no estado do veículo. Não demonstra que `CpmSyntheticPerceptionSource` consome o estado real, que existe uma política validada de tempo absoluto ITS ou que o encoder CPM produz uma mensagem CPM interoperável de ponta a ponta. Por isso, o estado global da Etapa 10.4 permanece PARTIAL.

O ficheiro global `scratch/docs/CPM/ETAPA_10_CPM_PLANO.md` não foi alterado.
