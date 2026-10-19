# SPATEM-19 / Fase 9.14 — Evidência integrada (2026-10-16)

## 1. Suite oficial

- Runner: `python3 scratch/test/run_tests.py --suite all`
- Ficheiros: 20
- Testes executados: 34
- Falhas: 0
- Erros: 0
- Resultado: PASS

## 2. Duplicação física e replay

Execução original: `test-results/spatem_9_14_duplicate_final_20261016_111246/`

- NS-3: exit code 0; `SHUTDOWN_COMPLETE` observado.
- SUMO/TraCI: exit code 0; 5 040 registos; 4 fases; publisher PASS.
- Hook de duplicação física: 124 eventos e 124 identificadores únicos.
- RAW_RX: 248.
- SPATEM_REPLAY_OK: 124.
- SPATEM_REPLAY_DROP: 124.
- SPATEM_DELIVER_OK: 124.
- Discrepâncias por identificador: 0.
- Invariante: duas receções, uma aceitação, uma rejeição da cópia e
  uma única entrega por pacote.

## 3. Emissor desconhecido

Execução original: `test-results/spatem_9_14_unknown_sender_current_20261016_110318/`

- NS-3: exit code 0; `SHUTDOWN_COMPLETE` observado.
- SUMO/TraCI: exit code 0; 5 040 registos; 4 fases; publisher PASS.
- Transmissões da RSU aceites: 117.
- `SENDER_NOT_FOUND`: 117.
- Dispatch SPATEM: 0.
- Replay accept/drop: 0/0.
- Entregas: 0.
- Resultado: rejeição do emissor desconhecido aprovada.

## 4. Identidade e self-origin

Na suite oficial, `test_spatem_rx_identity_binding.py` passou 4/4 casos:
identidade ausente, identidade incompatível, self-origin rejeitado antes
do replay e emissor remoto válido.

### SignalGroupID — domínio e fronteira SUMO

`SignalGroupID` tem domínio ASN.1 INTEGER (0..255), igual ao domínio de
`SpatemMovement::signalGroup` (`uint8_t`). A fronteira SUMO usa
`linkIndex` (`uint32_t`): o teste aceita 7, rejeita 256 e preserva o estado.
O caso cobre a validação antes da conversão; não existe um valor fora do
domínio que possa ser introduzido diretamente através de `signalGroup`.

## 5. Âmbito

Os resultados demonstram comportamento no ambiente de simulação ns-3/Vanetza
com estados de semáforo reais de SUMO/TraCI. Não constituem prova de
interoperabilidade com stacks externas nem de operação em hardware ITS-G5.
A Fase 9.15 continua reservada às regressões CAM/DENM.

## 6. Artefactos

- `duplicate_final_ns3.log`
- `duplicate_final_tls_publisher.log`
- `unknown_sender_ns3.log`
- `unknown_sender_tls_publisher.log`
- `official_suite.log`
- `SHA256SUMS.txt`
