#!/usr/bin/env bash
set -u

ROOT="$HOME/final/ns-3.47"
BIN="$ROOT/build/scratch/ns3.47-spatem_real_rx_pipeline_test-default"
RESULT_DIR="$ROOT/test-results/etapa9/9.8"
LOG="$RESULT_DIR/spatem_real_rx_pipeline.log"

mkdir -p "$RESULT_DIR"

echo "============================================================"
echo "ETAPA 9.8 — REAL SPATEM RX PIPELINE ACCEPTANCE"
echo "============================================================"

if [[ ! -x "$BIN" ]]; then
    echo "FAIL test executable not found: $BIN"
    exit 1
fi

"$BIN" >"$LOG" 2>&1
RUN_RC=$?

echo
echo "===== RUN RC ====="
echo "RUN_RC=$RUN_RC"

if [[ "$RUN_RC" -ne 0 ]]; then
    echo "FAIL real RX executable failed"
    echo "RESULTADO 9.8: FAIL"
    exit 1
fi

count()
{
    grep -c "$1" "$LOG" 2>/dev/null || true
}

socket_rx=$(count "SPATEM_REAL_RX_TEST.*socket_rx")
dispatch=$(count "EV=SPATEM_RX_DISPATCH")
btp_rx=$(count "EV=SPATEM_BTP_RX_OK")
verify=$(count "EV=SPATEM_VERIFY_OK")
decode=$(count "EV=SPATEM_DECODE_OK")
replay=$(count "EV=SPATEM_REPLAY_OK")
reception=$(count "EV=SPATEM_RECEPTION_OK")
deliver=$(count "EV=SPATEM_DELIVER_OK")

echo
echo "===== REAL RX COUNTS ====="
echo "SOCKET_RX=$socket_rx"
echo "SPATEM_RX_DISPATCH=$dispatch"
echo "SPATEM_BTP_RX_OK=$btp_rx"
echo "SPATEM_VERIFY_OK=$verify"
echo "SPATEM_DECODE_OK=$decode"
echo "SPATEM_REPLAY_OK=$replay"
echo "SPATEM_RECEPTION_OK=$reception"
echo "SPATEM_DELIVER_OK=$deliver"

fail()
{
    echo "FAIL $1"
    echo "RESULTADO 9.8: FAIL"
    exit 1
}

[[ "$socket_rx" -ge 1 ]] ||
    fail "real ns-3 socket RX evidence missing"

[[ "$dispatch" -ge 1 ]] ||
    fail "SPATEM BTP dispatch evidence missing"

[[ "$btp_rx" -ge 1 ]] ||
    fail "SPATEM BTP_RX evidence missing"

[[ "$verify" -ge 1 ]] ||
    fail "SPATEM VERIFY evidence missing"

[[ "$decode" -ge 1 ]] ||
    fail "SPATEM_DECODE_OK missing"

[[ "$replay" -ge 1 ]] ||
    fail "SPATEM_REPLAY_OK missing"

[[ "$reception" -ge 1 ]] ||
    fail "SPATEM_RECEPTION_OK missing"

[[ "$deliver" -ge 1 ]] ||
    fail "SPATEM_DELIVER_OK missing"

echo
echo "===== SEMANTIC SPATEM EVIDENCE ====="

grep -q \
  'EV=SPATEM_DECODE_OK.*station=1001.*intersections=1' \
  "$LOG" ||
  fail "decoded station/intersection-count evidence mismatch"

grep -q \
  'EV=SPATEM_RECEPTION_OK.*station=1001.*intersection=42' \
  "$LOG" ||
  fail "reception semantic evidence mismatch"

grep -q \
  'EV=SPATEM_DELIVER_OK.*station=1001.*intersection=42' \
  "$LOG" ||
  fail "delivery semantic evidence mismatch"

echo "PASS stationId=1001"
echo "PASS intersectionId=42"

echo
echo "===== RX PIPELINE CONTRACT ====="
echo "RAW_RX -> ACCESS_RX -> GEONET_RX -> BTP_RX -> VERIFY -> DECODE -> REPLAY -> RECEPTION -> DELIVER"

echo
echo "RESULTADO 9.8: PASS"
exit 0
