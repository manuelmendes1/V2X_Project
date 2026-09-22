---
status: current
type: tool-process
date: 2026-09-22
project: ns-3-vanetza-sumo
tags:
  - ns3
  - vanetza
  - v2x
  - denm
  - analyzer
  - validation
  - current
related:
  - [[scratch/vanetza_integration/docs/CURRENT_STATE]]
  - [[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
  - [[scratch/vanetza_integration/docs/vanetza_architecture]]
---

# v2x_analyzer — Current Analysis Tool

> **STATUS: CURRENT**
>
> **TYPE: TOOL / PROCESS**
>
> This README documents the current analyzer implementation.
> References to earlier logger/parser formats are historical regression
> context and must not be interpreted as the current operational log format.

Analisador modular de logs V2X (ns-3 + SUMO + Vanetza CAM + DENM), em Python.

## Novidades desta versão (face à v1.0.0 em `code_analyzer.txt`)

### 🔴 Achado N1 — 100% das linhas não reconhecidas (`analisador_output.txt`)

O `parser.py` anterior usava um único regex com **ordem de campos fixa**,
com `[SIM_TIME=...]` como campo **obrigatório** logo a seguir a
`[SIM_STEP=...]`. O log real gerado pela build atual não inclui
`[SIM_TIME=...]` e usa uma ordem de campos diferente
(`SIM_STEP,VEH,NODE,STATION,SEQ,LVL,MOD,EV`, não
`SIM_STEP,SIM_TIME,SEQ,VEH,NODE,STATION,...`). Resultado: 24661/24661
linhas não reconhecidas.

Não é a primeira vez que o formato do logger muda — só nos ficheiros
deste projeto há **três** variantes distintas de `V2XWriteLog`
(`estrutura_13.txt`: `[STEP=][T=...s]`; `up_code_v2.txt`:
`[SIM_STEP=][SIM_TIME=...s]`; log real: `[SIM_STEP=]` sem tempo
nenhum). Um regex preso a uma ordem fixa parte de novo assim que isto
voltar a acontecer.

**Correção estrutural:** `parser.py` foi reescrito para extrair pares
`[CHAVE=VALOR]` **independentemente da ordem**, com uma tabela de
aliases (`SIM_STEP`/`STEP`, `SIM_TIME`/`T`) para os nomes que já
mudaram. Campos desconhecidos são ignorados, não partem o parsing —
robustez a favor de campos novos que apareçam no futuro.

### 🔴 Achados de contagem (encontrados ao reescrever, nunca antes detetados porque o parser já falhava primeiro)

| # | Módulo | Problema | Efeito |
|---|---|---|---|
| N2 | `rx_pipeline.py`, `timing.py` | `decode_ok`/`decode_fail_empty`/`decode_fail_asn1` amarrados ao módulo `"CAM"`; o módulo real de `cam_decoder.cc` é `"CAM_RX"` (a própria docstring do ficheiro já o dizia — só o dicionário estava errado) | Estas 3 métricas ficavam **sempre a 0**, mesmo com parsing perfeito |
| N3 | `tx_pipeline.py` | `encode_empty` apontava para `(CAM,ENCODE_EMPTY)`, que não existe em código nenhum; o evento real é `(CAM_TX,EMPTY)` | `encode_empty` nunca incrementava |
| N4 | `rx_pipeline.py`, `errors.py` | `"NO_CERT"` (código mais antigo) vs `"NO_CERTIFICATE"` (código mais recente) | Subcontagem consoante o binário compilado |
| N5 | `rx_pipeline.py` | `"VERIFY_RESULT_FAIL"` (mais antigo) vs `"VERIFY_FAIL"` ao nível do módulo `RX` (mais recente) — não confundir com `(SECURITY,VERIFY_FAIL)` | idem |
| N6 | `connectivity.py`, `vehicles.py` | `"PIPELINE_OK"` (`RX`) vs `"RECEPTION_OK"` (`CAM_RX`, código mais recente) — o evento de sucesso final do pipeline | Conectividade/entrega podiam ficar a 0/N.A. consoante a variante |
| N7 | **(crítico)** `connectivity.py`, `vehicles.py` | As duas variantes de `rx_pipeline.cc` usam convenções **opostas** de `VEH=`/`STATION=` neste evento (`PIPELINE_OK`: VEH=emissor, STATION=recetor; `RECEPTION_OK`: VEH=recetor, STATION=emissor — ver `identity.py` para a prova linha-a-linha). Tratar `VEH=` sempre como "o emissor" faz emissor e recetor colapsarem no mesmo veículo para logs da 2ª convenção | **Zero pares ativos**, mesmo com conectividade real perfeita |
| N8 | `identity.py` | Mapa de identidade construído só a partir de `TX_READY` | Um veículo que nunca conseguisse transmitir desaparecia de N — a mesma classe de bug que já distorceu a entrega efetiva de ~37% para 56% no histórico deste projeto |
| N9 | `exit_criteria.py` | `"Duplicados = 0"` sempre `True`, sem verificação real | Critério de saída da Etapa 4 dava PASS sem prova nenhuma |

Todos os achados N2–N9 são acompanhados de teste de regressão dedicado
em `tests/test_metrics.py`, replicando literalmente o cenário que
provou o bug (não só "existe um teste", mas "este teste falha se o bug
voltar").

### ✅ Novo: deteção real de duplicados (`metrics/duplicates.py`)

Um duplicado = a mesma tripla (nó recetor, identificador do emissor,
seqId) a atingir sucesso final do pipeline mais do que uma vez.
`seqId<=0` é sentinela (nunca atribuído a uma CAM real —
`CamTransmission::PrepareTransmission` pré-incrementa o contador antes
do primeiro envio) e é ignorado. O identificador do emissor é resolvido
via `identity.resolve_sender_receiver`, não via `e.station` bruto —
necessário por causa do achado N7 acima (`e.station` só identifica o
emissor de forma fiável numa das duas convenções).

### ✅ Melhorado: `identity.py`

Construído a partir de `(REGISTRY,NODE_OK)`/`(REGISTRY,STATION_OK)`
(emitidos na criação do veículo, antes de qualquer transmissão) em vez
de só `(CAM,TX_READY)`. `(REGISTRY,CREATE_OK)` é ignorado como fonte —
essa linha é emitida antes da identidade ser atribuída, com
`stationId` ainda ao valor por omissão (o sentinela `-1` num campo
`uint64_t`, que aparece como `18446744073709551615` no log real).

### ✅ `LogEntry.duration_us()` e `.reason()` com fallback de texto livre

`DURATION_US` e `reason` chegam de duas formas consoante a API do
logger: como campo `[DURATION_US=N]`/`[REASON=...]` (API "preferred",
`PacketContext`) ou embutidos em texto livre (API "legacy" — a que
gera quase todos os dados observados). Sem este fallback, a secção de
durações do relatório ficaria "sem amostras" para sempre, mesmo depois
de todos os outros bugs corrigidos.

### ✅ `aliases.py` — fonte única para grupos de (módulo,evento) equivalentes

Antes desta versão, cada módulo de métricas mantinha a sua própria
cópia (parcial, e por vezes desincronizada — foi exatamente isso que
causou o achado N2) das variantes de nome conhecidas. Agora há um único
sítio (`v2x_analyzer/aliases.py`) para isto, com a evidência de
código-fonte citada para cada grupo.

## Instalar e correr

Só biblioteca padrão do Python 3.10+ — nada para instalar para usar.

```bash
python3 -m v2x_analyzer output_log.txt
python3 -m v2x_analyzer output_log_001 output_log_002   # vários ficheiros, pela ordem dada
python3 -m v2x_analyzer --json output_log.txt            # para consumo programático
python3 -m v2x_analyzer -o relatorio.txt output_log.txt   # gravar em ficheiro
```

Código de saída: `0` se o relatório passa todos os critérios atuais, `1` caso
contrário.

## Estrutura

```
v2x_analyzer/
    parser.py            — LogEntry + parsing independente de ordem/aliases (achado N1)
    aliases.py            — grupos de (módulo,evento) equivalentes entre variantes do logger
    identity.py            — mapa vehId<->nodeId<->stationId + resolve_sender_receiver (achados N7/N8)
    metrics/
        tx_pipeline.py     — TX taxonomy: Generate/Encode/Sign/BTP/GeoNet/Access/Send/Complete
        trigger_reasons.py — motivo de disparo do ShouldSendCAM
        rx_pipeline.py      — RX taxonomy: Raw/Access/GeoNet/BTP/Verify/Decode/Replay/Reception/Deliver
        errors.py           — Sender/Receiver Not Found, Self-RX, No Cert (achado N4)
        connectivity.py     — pares ativos, entrega efetiva, fan-out (achados N6/N7)
        vehicles.py          — TX/RX por veículo, ghost check (achados N6/N7)
        timing.py            — estatísticas de DURATION_US (achado N2)
        duplicates.py         — deteção real de duplicados (achado N9, módulo NOVO)
        exit_criteria.py     — critérios de saída da taxonomia atual
    report.py            — junta tudo, formata texto e JSON
    cli.py / __main__.py — linha de comandos
    tests/                — 26 testes validados
```

## Testes

```bash
pip install pytest --break-system-packages
python3 -m pytest v2x_analyzer/tests/ -v
```

26 testes atualmente: parsing (incluindo as 5 linhas reais do
`analisador_output.txt` como regressão literal do achado N1, mais
independência de ordem de campos e aliases STEP/SIM_STEP e T/SIM_TIME),
métricas (regressões dos bugs históricos do bash **e** dos achados
N2–N9 desta sessão), e um teste de integração ponta-a-ponta contra
`tests/fixtures/small_run.log`.

**Nota sobre o fixture de integração:** o `small_run.log` da versão
anterior não foi incluído nos ficheiros fornecidos nesta sessão (só o
código Python foi fornecido, não os dados de teste). O fixture atual é
**novo**, escrito de propósito, com todos os números calculados à mão
— ver comentários no topo de `tests/test_integration.py`.

## Limitações conhecidas / por fazer

- A variante `(CAM_RX, PIPELINE_OK)` em `aliases.PIPELINE_FINAL_OK` é
  especulativa — nunca foi confirmada em nenhum código-fonte fornecido
  (só `(RX,PIPELINE_OK)` e `(CAM_RX,RECEPTION_OK)` foram). Foi incluída
  por segurança, tratada com a convenção "antiga" (VEH=emissor). Se
  vier a aparecer em código real com a convenção "nova", terá de ser
  movida para o ramo correto de `identity.resolve_sender_receiver`.
- `cam_framing.cc` tem eventos `FRAME_BEGIN`/`FRAME_UNPACK_BEGIN` sem
  `DURATION_US` associado (ao contrário de encode/sign/verify/decode).
  `metrics/timing.py` não os inclui — decisão pendente herdada da
  versão anterior: acrescentar duração ali também, ou remover os
  marcadores órfãos.
- `NOT_IMPLEMENTED` em `errors.py` (só `TS_EXPIRED` agora — `DUPLICATE`
  passou a ser verificado de facto nesta versão).
- **Recomendação forte:** os três ficheiros de código fornecidos nesta
  sessão (`estrutura_13.txt`, `up_code_v2.txt`, e o formato do log
  real) discordam entre si em pelo menos 4 pontos (nomes de evento,
  presença de SIM_TIME, convenção VEH=/STATION= em PIPELINE_OK vs
  RECEPTION_OK). Vale a pena confirmar, por recompilação a partir do
  código exato do `scratch/` atual, qual das variantes está de facto a
  gerar os logs — este analisador aceita ambas, mas a dissertação
  precisa de saber qual é a verdade de facto (mesma lição do achado
  crítico #1 original: nunca assumir que um log corresponde ao código
  atual sem reproduzir).
## Estado atual — P3 e DENM

### Taxonomia operacional

```text
TX: GENERATE → ENCODE → SIGN → BTP_TX → GEONET_TX → ACCESS_TX → SEND → TX_COMPLETE

RX: RAW_RX → ACCESS_RX → GEONET_RX → BTP_RX → VERIFY → DECODE → REPLAY → RECEPTION → DELIVER
```

### Normalização DENM

```text
DENM/GENERATE_OK       → GENERATE
DENM/ENCODE_OK         → ENCODE
SECURITY/SIGN_BEGIN    → SIGN
SECURITY/SIGN_OK       → SIGN
DENM/DENM_DECODE_OK    → DECODE
DENM/DENM_REPLAY_OK    → REPLAY
DENM/DENM_RECEPTION_OK → RECEPTION
DENM/DENM_DELIVER_OK   → DELIVER
```

### Evidência de validação

Log de referência: `output_log_etapa_8_10_v2.txt`

```text
Analyzer tests:           26/26 PASS
Real-log analyzer:        RESULTADO P3: PASS
Analyzer exit code:       0
Structured DENM SIGN_OK:  4
TX:                       106 em todas as 8 etapas
RX:                       154 em todas as 9 etapas
```

### Relações documentais

- [[scratch/vanetza_integration/docs/CURRENT_STATE]]
- [[scratch/vanetza_integration/docs/DOCUMENT_STATUS]]
- [[scratch/vanetza_integration/docs/vanetza_architecture]]
- [[scratch/vanetza_integration/docs/historical/project_snapshots/code_analyzer]]
