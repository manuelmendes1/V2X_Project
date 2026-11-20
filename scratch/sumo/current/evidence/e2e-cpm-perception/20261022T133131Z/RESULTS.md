# Evidência E2E CPM e ingestão DENM

Data do arquivo: 20261022T133131Z (UTC).

## E2E CPM — fronteira temporal SUMO

Origem temporária dos logs: `/tmp/cpm-step-boundary-e2e.GmDhij/`.

- Processo ns-3: exit code 0.
- Bridge SUMO: exit code 0.
- Datagramas de veículos enviados: 200.
- Estados geodésicos aplicados: 200.
- Timestamps lógicos distintos aplicados: 150.
- Mensagens CPM codificadas: 150.
- Timestamps CPM distintos: 150.
- Codificações duplicadas: 0.
- Timestamps aplicados sem CPM: 0.
- Intervalos de timestamp diferentes de 100 ms: 0.
- Timestamp máximo: 15000000 µs.
- Codificações rejeitadas: 0.

## Ensaio de fronteira — ingestão DENM

Origem temporária dos logs: `/tmp/cpm-denm-boundary.6QGuW6/`.

- Processo ns-3: exit code 0.
- Injector: exit code 0.
- Estados enviados: 20.
- Marcadores `SUMO_STEP_END` enviados: 10.
- Evento DENM enviado e ingerido: 1.
- Timestamps CPM: 100000 a 1000000 µs, dez valores únicos.
- Codificações rejeitadas: 0.
- Scheduler DENM desativado: não foi testada a submissão/transmissão DENM.

## Limites

Este arquivo preserva os logs das execuções e resume os resultados observados
nos respetivos outputs de terminal. Não demonstra transmissão CPM, cobertura
integral CPM R2, interoperabilidade nem validação completa do scheduler DENM.
