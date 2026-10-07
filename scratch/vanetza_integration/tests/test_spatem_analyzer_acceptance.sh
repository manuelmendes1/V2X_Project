#!/usr/bin/env bash
set -u

ROOT="$HOME/final/ns-3.47"
LOG="$ROOT/test-results/etapa9/9.10/spatem_real_tx_context_acceptance.log"
OUT="$ROOT/test-results/etapa9/9.11_spatem_analyzer_acceptance.log"

cd "$ROOT" || exit 1

mkdir -p "$ROOT/test-results/etapa9"

echo "============================================================"
echo "ETAPA 9.11 — SPATEM ANALYZER ACCEPTANCE"
echo "============================================================"

if [[ ! -f "$LOG" ]]; then
    echo "FAIL: real SPATEM log not found: $LOG"
    exit 1
fi

PYTHONPATH=scratch python3 -m v2x_analyzer "$LOG" > "$OUT" 2>&1
ANALYZER_RC=$?

echo "ANALYZER_RC=$ANALYZER_RC"

if [[ "$ANALYZER_RC" -ne 0 ]]; then
    echo "FAIL: analyzer returned $ANALYZER_RC"
    exit 1
fi

python3 - "$LOG" "$OUT" <<'PY'
import sys
from pathlib import Path

sys.path.insert(0, "scratch")

from v2x_analyzer.parser import load_log
from v2x_analyzer.identity import build_identity_map, resolve_sender_receiver
from v2x_analyzer.metrics import tx_pipeline, rx_pipeline

log = Path(sys.argv[1])
report = Path(sys.argv[2])

entries, _ = load_log([str(log)])
text = report.read_text(encoding="utf-8")

def require(label, condition):
    if not condition:
        print(f"FAIL: {label}")
        raise SystemExit(1)
    print(f"PASS: {label}")

spatem = [e for e in entries if e.module == "SPATEM"]
require("10 SPATEM protocol events observed", len(spatem) == 10)

expected_events = {
    "SPATEM_GENERATE_OK",
    "SPATEM_ENCODE_OK",
    "SPATEM_RX_DISPATCH",
    "SPATEM_GEONET_RX_OK",
    "SPATEM_BTP_RX_OK",
    "SPATEM_VERIFY_OK",
    "SPATEM_DECODE_OK",
    "SPATEM_REPLAY_OK",
    "SPATEM_RECEPTION_OK",
    "SPATEM_DELIVER_OK",
}
require(
    "all expected SPATEM events observed",
    {e.event for e in spatem} == expected_events,
)

tx = tx_pipeline.compute(entries)
rx = rx_pipeline.compute(entries)

require("TX Generate OK=1", tx.generate_ok == 1)
require("TX Encode OK=1", tx.encode_ok == 1)
require("TX BTP TX OK=1", tx.btp_tx_ok == 1)
require("TX GeoNet TX OK=1", tx.geonet_tx_ok == 1)
require("TX Access TX OK=1", tx.access_tx_ok == 1)
require("TX Send OK=1", tx.send_ok == 1)
require("TX Complete=1", tx.tx_complete == 1)

require("RX Raw RX=1", rx.raw_rx == 1)
require("RX Access RX OK=1", rx.access_rx_ok == 1)
require("RX GeoNet RX OK=1", rx.geonet_rx_ok == 1)
require("RX BTP RX OK=1", rx.btp_rx_ok == 1)
require("RX Verify OK=1", rx.verify_ok == 1)
require("RX Decode OK=1", rx.decode_ok == 1)
require("RX Replay OK=1", rx.replay_ok == 1)
require("RX Reception OK=1", rx.reception_ok == 1)
require("RX Deliver OK=1", rx.deliver_ok == 1)

identity = build_identity_map(entries)
delivery = [
    e for e in entries
    if e.is_("SPATEM", "SPATEM_DELIVER_OK")
]

require("exactly one SPATEM delivery", len(delivery) == 1)

sender, receiver = resolve_sender_receiver(delivery[0], identity)

require(
    "SPATEM delivery receiver resolves to veh1",
    receiver == "veh1",
)

require(
    "SPATEM delivery does not invent an unresolved sender",
    sender is None,
)

for criterion in (
    "Cadeia TX consistente",
    "Cadeia RX consistente",
    "Sem falhas TX/RX observadas",
    "Veículos identificados",
    "Duplicados",
):
    require(
        f"report criterion PASS: {criterion}",
        f"[PASS] {criterion}" in text,
    )

for legacy in ("PIPELINE_OK", "NO_CERT", "TX_SENT"):
    require(
        f"historical event absent: {legacy}",
        legacy not in text,
    )

print()
print("SPATEM_ANALYZER_ACCEPTANCE=PASS")
print("RESULTADO 9.11: CLOSED")
PY

RC=$?

echo
echo "ACCEPTANCE_RC=$RC"
echo "REPORT=$OUT"

exit "$RC"
